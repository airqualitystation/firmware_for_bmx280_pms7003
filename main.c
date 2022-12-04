/*
 * Read pediodically the measurements readen from the endpoint's sensors
 * then send to the LoRaWAN network in which the endpoint is registered.
 * 
 * Copyright (C) 2020-2022 LIG Université Grenoble Alpes
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * @author Didier DONSEZ
 */

#define ENABLE_DEBUG (1)
#include "debug.h"

#include <string.h>

//#include "xtimer.h"
#include <time.h>

#include "ztimer.h"
#include "ztimer/periodic.h"

#include "mutex.h"
#include "periph_conf.h"
#include "periph/rtc.h"
#include "periph/pm.h"

#include "cpu_conf.h"
#include "periph/cpuid.h"

#include "board.h"

#include "fmt.h"

//#include "net/loramac.h"
#include "semtech_loramac.h"
#include "loramac_utils.h"

#include "git_utils.h"
//#include "wdt_utils.h"
#include "wdt_ztimer.h"

#include "sensors.h"

#include "app_clock.h"

#include <random.h>

/* Declare globally the loramac descriptor */
extern semtech_loramac_t loramac;

// Count the number of elements in an array.
#define CNT(array) (uint8_t)(sizeof(array) / sizeof(*array))

/* LoRaMac values */
#define JOIN_NEXT_RETRY_TIME            10 // Next join tentative in 1 minute
#define SECONDS_PER_DAY                 24 * 60 * 60

/* Use a fast datarate, e.g. BW125/SF7 in EU868 */
#define DR_INIT                         LORAMAC_DR_5

#define FIRST_TX_PERIOD                 TXPERIOD_AT_DR0
#define TX_PERIOD                       TXPERIOD_AT_DR0

#define PORT_UP_DATA                    101
#define PORT_UP_ERROR                   102

#define PORT_DN_TEXT                    101
#define PORT_DN_SET_TX_PERIOD           3
#define PORT_DN_REBOOT_NOW           	64
#define PORT_DN_REBOOT_ONE_MINUTE       65
#define PORT_DN_REBOOT_ONE_HOUR         66

/* Implement the receiver thread */
#define RECEIVER_MSG_QUEUE                          (4U)

#if OTAA == 1

#ifdef FORGE_DEVEUI_APPEUI_APPKEY
static uint8_t secret[LORAMAC_APPKEY_LEN];
#endif

static uint8_t deveui[LORAMAC_DEVEUI_LEN] ;
static uint8_t appeui[LORAMAC_APPEUI_LEN] ;
static uint8_t appkey[LORAMAC_APPKEY_LEN] ;

#else

// static uint32_t* devaddr = { DEVADDRS };

static uint8_t devaddr[LORAMAC_DEVADDR_LEN] ;
static uint8_t appskey[LORAMAC_NWKSKEY_LEN] ;
static uint8_t nwkskey[LORAMAC_APPSKEY_LEN] ;

#endif

static msg_t _receiver_queue[RECEIVER_MSG_QUEUE];
static char _receiver_stack[THREAD_STACKSIZE_DEFAULT];

static uint16_t tx_period = TX_PERIOD;



#if GUARD_SENDER_WAKEUP == 1

// Add a guard since the sender thread is stuck into the semtech_loramac_send call

// TODO add a loramac_utils_guarded_semtech_loramac_send

static ztimer_now_t start_time=0;

static kernel_pid_t sender_pid;

static ztimer_periodic_t _wakeup_sender_ztimer; // = { 0 };

static bool _wakeup_sender_cb(void *arg)
{
	(void)arg;
    //printf("[%s] enter\n", __FUNCTION__);
    if(start_time != 0)
    {
    	ztimer_now_t delta = ztimer_now(ZTIMER_MSEC) - start_time;
        if(delta > 60000)
        {
            DEBUG("[%s] timeout after %ld ms\n", __FUNCTION__, delta);
#if 0
            DEBUG("[%s] wake up sender\n", __FUNCTION__);
            thread_wakeup(sender_pid); // REM: thread_wakeup seems to have no effect
#else
            DEBUG("[%s] rebooting now ...\n", __FUNCTION__);
            pm_reboot();
#endif
        } else {
            DEBUG("[%s] delta: %ld ms\n", __FUNCTION__, delta);
        }
    }
	// function to call on each trigger returns true if the timer should keep going
    return true;
}

static int wakeup_sender_ztimer(void) {

	// check the guard every 10 seconds
    ztimer_periodic_init(ZTIMER_SEC, &_wakeup_sender_ztimer, _wakeup_sender_cb, (void*)NULL, 10);
    ztimer_periodic_start(&_wakeup_sender_ztimer);

    if (!ztimer_is_set(ZTIMER_SEC, &_wakeup_sender_ztimer.timer)) {
    	printf("[%s] ERROR\n", __FUNCTION__);
        return -1;
    }
    DEBUG("[%s] started\n", __FUNCTION__);

	return 0;
}

#endif



static bool rebooting = false;

static void sender(void)
{

#if GUARD_SENDER_WAKEUP == 1
	sender_pid = thread_getpid();
    wakeup_sender_ztimer();
#endif

    uint32_t cnt_sent_messages=0;
    uint8_t payload[241];

    semtech_loramac_set_class(&loramac, ENDPOINT_CLASS);
    semtech_loramac_set_adr(&loramac, ADR_ON);

#if APP_CLOCK_SYNC == 1
	// request for clock synchronization
	app_clock_send_app_time_req(&loramac);
    cnt_sent_messages++;

    sleep_period_dr(&loramac);
#endif

    while(!rebooting) {
        	DEBUG("[sender] Encoding payload ...\n");
            //start_time = ztimer_now(ZTIMER_MSEC);
        	uint8_t size = encode_sensors(payload);
            //uint32_t duration =  ztimer_now(ZTIMER_MSEC) - start_time;
        	//DEBUG("[sender] Payload encoded duration=%ld\n", duration);
        	DEBUG("[sender] Payload size=%d payload=", size);
            printf_ba(payload, size);
        	DEBUG("\n");

        	DEBUG("[sender] Send @ port=%d size=%d\n", DATA_PORT, size);

            // WARNING : If LORAMAC_TX_CNF, the firmware is blocked when the network server does not confirmed the message
            semtech_loramac_set_tx_mode(&loramac, TXCNF ? LORAMAC_TX_CNF : LORAMAC_TX_UNCNF);
            semtech_loramac_set_tx_port(&loramac, DATA_PORT);

#if GUARD_SENDER_WAKEUP == 1
            start_time = ztimer_now(ZTIMER_MSEC);
#endif
            uint8_t ret = semtech_loramac_send(&loramac, payload, size);

#if GUARD_SENDER_WAKEUP == 1
            uint32_t duration = ztimer_now(ZTIMER_MSEC) - start_time;
            start_time = 0;
#endif
            if (ret == SEMTECH_LORAMAC_TX_DONE || ret == SEMTECH_LORAMAC_TX_CNF_FAILED) {
                uint32_t uplink_counter = semtech_loramac_get_uplink_counter(&loramac);
#if GUARD_SENDER_WAKEUP == 1
            	DEBUG("[sender] Tx Done ret=%d fcnt=%ld duration=%ld\n", ret, uplink_counter, duration);
#else
            	DEBUG("[sender] Tx Done ret=%d fcnt=%ld\n", ret, uplink_counter);
#endif
            	if(ret == SEMTECH_LORAMAC_TX_CNF_FAILED) {

                    DEBUG("[sender] message was transmitted but no ACK  was received for Confirmed\n");
                }
                cnt_sent_messages++;

            } else {
                /*
                * @return SEMTECH_LORAMAC_NOT_JOINED when the network is not joined
                * @return SEMTECH_LORAMAC_BUSY when the mac is already active (join or tx in progress)
                * @return SEMTECH_LORAMAC_DUTYCYCLE_RESTRICTED when the send is rejected because of dutycycle restriction
                * @return SEMTECH_LORAMAC_TX_ERROR when an invalid parameter is given
                */
#if GUARD_SENDER_WAKEUP == 1
                DEBUG("[sender] ERROR: Cannot send payload: ret code: %d (%s) duration=%ld\n", ret, loramac_utils_err_message(ret), duration);
#else
                DEBUG("[sender] ERROR: Cannot send payload: ret code: %d (%s)\n", ret, loramac_utils_err_message(ret));
#endif
            }
            loramac_utils_sleep_adaptative_period_dr(&loramac, TX_PERIOD);

#if APP_CLOCK_SYNC == 1
            // send a APP_TIME_REQ request every APP_TIME_REQ_PERIOD message
            if(cnt_sent_messages%APP_TIME_REQ_PERIOD == 0)
            {
            	app_clock_send_app_time_req(&loramac);
                cnt_sent_messages++;

                loramac_utils_sleep_adaptative_period_dr(&loramac, TX_PERIOD);
            }
#endif
    }

	DEBUG("[sender] Exiting ...\n");

	// TODO send a message for confirming the reboot

    return;
}

static void *receiver(void *arg)
{
    msg_init_queue(_receiver_queue, RECEIVER_MSG_QUEUE);

    (void)arg;
    while (1) {
        app_clock_print_rtc();

        /* blocks until something is received */
        switch (semtech_loramac_recv(&loramac)) {
            case SEMTECH_LORAMAC_RX_DATA:
                // TODO process Downlink payload
                switch(loramac.rx_data.port) {
                    case PORT_DN_TEXT:
                        loramac.rx_data.payload[loramac.rx_data.payload_len] = 0;
                        DEBUG("[dn] Data received: text=%s, port: %d\n",
                            (char *)loramac.rx_data.payload, loramac.rx_data.port);
                        break;
                    case PORT_DN_SET_TX_PERIOD:
                        if(loramac.rx_data.payload_len == sizeof(tx_period)) {
                        	memcpy(&tx_period, loramac.rx_data.payload, sizeof(uint16_t));
                            DEBUG("[dn] Data received: tx_period=%d, port: %d\n",
                                tx_period, loramac.rx_data.port);
                        } else {
                            DEBUG("[dn] Data received: bad size for tx_period, port: %d\n",
                                 loramac.rx_data.port);
                        }
                        break;
                    case APP_CLOCK_PORT:
                    	(void)app_clock_process_downlink(&loramac);
                    	break;

                    case PORT_DN_REBOOT_NOW:
                        DEBUG("[dn] Reboot now. port: %d\n", loramac.rx_data.port);
                        rebooting = true;
            			pm_reboot();
                    	break;
                    case PORT_DN_REBOOT_ONE_MINUTE:
                        DEBUG("[dn] Reboot in 60 sec. port: %d\n", loramac.rx_data.port);
                        rebooting = true;
                        ztimer_sleep(ZTIMER_SEC, 60U);
            			pm_reboot();
                    	break;
                    case PORT_DN_REBOOT_ONE_HOUR:
                        DEBUG("[dn] Reboot in 3600 sec. port: %d\n", loramac.rx_data.port);
                        rebooting = true;
                        ztimer_sleep(ZTIMER_SEC, 3600U);
            			pm_reboot();
                    	break;

                    default:
                        DEBUG("[dn] Data received: ");
                        printf_ba(loramac.rx_data.payload, loramac.rx_data.payload_len);
                        DEBUG(", port: %d\n",loramac.rx_data.port);
                        break;
                }
                break;

			case SEMTECH_LORAMAC_RX_LINK_CHECK:
				DEBUG("[dn] Link check information:\n"
				   "  - Demodulation margin: %d\n"
				   "  - Number of gateways: %d\n",
				   loramac.link_chk.demod_margin,
				   loramac.link_chk.nb_gateways);
				break;

			case SEMTECH_LORAMAC_RX_CONFIRMED:
				DEBUG("[dn] Received ACK from network\n");
				break;

			case SEMTECH_LORAMAC_TX_SCHEDULE:
				DEBUG("[dn] The Network Server has pending data\n");
				break;

            default:
                break;
        }
    }
    return NULL;
}

static void cpuid_info(void) {
	uint8_t id[CPUID_LEN];
	/* read the CPUID */
	cpuid_get(id);
	DEBUG("[info] CpuId:"); printf_ba(id,CPUID_LEN); DEBUG("\n");
}

static void loramac_info(void) {
#ifdef OPERATOR
    DEBUG("[info] Operator: %s\n", OPERATOR);
#endif
#ifdef LABEL
    DEBUG("[info] Label: %s\n",LABEL);
#endif
    DEBUG("[mac] Region: " LORAMAC_REGION_STR "\n");
#if EU868_DUTY_CYCLE_ENABLED == 0
    DEBUG("[mac] DutyCycle: disabled\n");
#else
    DEBUG("[mac] DutyCycle: enabled\n");
#endif
}

//static char* wdt_cmdline[] = {"wdt","start"};

int main(void)
{

	git_cmd(0, NULL);
	//wdt_cmd(2, wdt_cmdline);
#if ENABLE_WDT_ZTIMER == 1
	start_wdt_ztimer();
#endif


#if APP_CLOCK_SYNC == 1
    app_clock_print_rtc();
#endif
    cpuid_info();
    loramac_info();

    /* initialize the sensors */
    init_sensors();

    /* initialize the loramac stack */
    //semtech_loramac_init(&loramac);

#if OTAA == 1

#ifdef FORGE_DEVEUI_APPEUI_APPKEY
    /* forge the deveui, appeui and appkey of the endpoint */
    fmt_hex_bytes(secret, SECRET);
    loramac_utils_forge_euis_and_key(deveui,appeui,appkey,secret);
    DEBUG("[otaa] Secret:"); printf_ba(secret,LORAMAC_APPKEY_LEN); DEBUG("\n");
#else
    /* Convert identifiers and application key */
    fmt_hex_bytes(deveui, DEVEUI);
    fmt_hex_bytes(appeui, APPEUI);
    fmt_hex_bytes(appkey, APPKEY);
#endif
	DEBUG("[otaa] DevEUI:"); printf_ba(deveui,LORAMAC_DEVEUI_LEN); DEBUG("\n");
    DEBUG("[otaa] AppEUI:"); printf_ba(appeui,LORAMAC_APPEUI_LEN); DEBUG("\n");
    DEBUG("[otaa] AppKey:"); printf_ba(appkey,LORAMAC_APPKEY_LEN); DEBUG("\n");

    /* set the LoRaWAN keys */
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    /* start the OTAA join procedure (and retries in required) */
    /*uint8_t joinRes = */ loramac_utils_join_retry_loop(&loramac, DR_INIT, JOIN_NEXT_RETRY_TIME, SECONDS_PER_DAY);

    //random_init_by_array(uint32_t init_key[], int key_length)
    random_init_by_array((void*)appkey, LORAMAC_APPKEY_LEN/sizeof(uint32_t));

#else
    /* Convert identifiers and application key */

	// uint32_t devaddr = devaddrs[0];

    fmt_hex_bytes(devaddr, DEVADDR);
    fmt_hex_bytes(appskey, APPSKEY);
    fmt_hex_bytes(nwkskey, NWKSKEY);

    DEBUG("[abp] DevAddr:"); printf_ba(devaddr,LORAMAC_DEVADDR_LEN); DEBUG("\n");
    DEBUG("[abp] AppSKey:"); printf_ba(appskey,LORAMAC_NWKSKEY_LEN); DEBUG("\n");
    DEBUG("[abp] NwkSKey:"); printf_ba(nwkskey,LORAMAC_APPSKEY_LEN); DEBUG("\n");

    /* set the LoRaWAN keys */
    semtech_loramac_set_devaddr(&loramac, devaddr);
    semtech_loramac_set_appskey(&loramac, appskey);
    semtech_loramac_set_nwkskey(&loramac, nwkskey);


    /* start the ABP join procedure (and retries in required) */
    /*uint8_t joinRes = */ loramac_utils_abp_join_retry_loop(&loramac, DR_INIT, JOIN_NEXT_RETRY_TIME, SECONDS_PER_DAY);

    //random_init_by_array(uint32_t init_key[], int key_length)
    random_init_by_array((void*)appskey, LORAMAC_APPSKEY_LEN/sizeof(uint32_t));

#endif

#ifdef FCNT_UP
    semtech_loramac_set_uplink_counter(&loramac, FCNT_UP);
#endif

    /* start the receiver thread */
    thread_create(_receiver_stack, sizeof(_receiver_stack),
                  THREAD_PRIORITY_MAIN - 1, 0, receiver, NULL, "RECEIVER");

    /* sleep */
    loramac_utils_sleep_adaptative_period_dr(&loramac, FIRST_TX_PERIOD);


    /* call the sender */
    sender();
    
    return 0; /* should never be reached */
}
