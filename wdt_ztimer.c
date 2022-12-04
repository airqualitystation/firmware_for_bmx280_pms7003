/*
 * Copyright (C) 2020-2022 Université Grenoble Alpes
 */

/*
 * Author: Didier Donsez, Université Grenoble Alpes
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef ENABLE_DEBUG_WDT_TIMER
#define ENABLE_DEBUG_WDT_TIMER	0
#endif

#define ENABLE_DEBUG	(ENABLE_DEBUG_WDT_TIMER)
#include "debug.h"

#include "ztimer.h"
#include "ztimer/periodic.h"

#include "fmt.h"
#include "shell.h"

#ifndef WDT_UTILS_KICK_PERIOD
#define WDT_UTILS_KICK_PERIOD		4000LU	// msec
#endif

#ifndef WDT_UTILS_TIMEOUT
#define WDT_UTILS_TIMEOUT			10000 	// msec
#endif

#include "periph/wdt.h"

static unsigned cpt = 0;

static const char* _param = "WDT";

static bool _wdt_kick_cb(void *arg)
{
	(void)arg;
	cpt++;
    DEBUG("\n[%s] KICK %s %d\n", __FUNCTION__, (char*)arg, cpt);
    wdt_kick();

	// function to call on each trigger returns true if the timer should keep going
    return true;
}

static ztimer_periodic_t _wdt_ztimer;

int start_wdt_ztimer(void) {

    ztimer_periodic_init(ZTIMER_MSEC, &_wdt_ztimer, _wdt_kick_cb, (void*)_param, WDT_UTILS_KICK_PERIOD);
    ztimer_periodic_start(&_wdt_ztimer);
	wdt_setup_reboot(0, WDT_UTILS_TIMEOUT);
	wdt_start();

    if (!ztimer_is_set(ZTIMER_MSEC, &_wdt_ztimer.timer)) {
    	printf("[%s] ERROR\n", __FUNCTION__);
        return -1;
    } else {
#if	ENABLE_DEBUG == 1
    	printf("[%s] WDT started (DEBUG mode)\n", __FUNCTION__);
#else
		printf("[%s] WDT started (SILENT mode)\n", __FUNCTION__);
#endif
		printf("[%s] WDT kick period %ld msec\n", __FUNCTION__, WDT_UTILS_KICK_PERIOD);
		printf("[%s] WDT timeout %d msec\n", __FUNCTION__, WDT_UTILS_TIMEOUT);
		return 0;
    }

	return 0;
}

// TODO int stop_wdt_timer(void);

#if WDT_HAS_STOP
int wdt_stop_cmd(int argc, char *argv[]) {
	(void) argc;
	(void) argv;
	puts("WDT stopped");
	wdt_stop();
	return 0;
}
#endif

int abort_cmd(int argc, char *argv[]) {
	(void) argc;
	(void) argv;
	puts("Abort now !");
	abort();
	return 0;
}

#if 0
static const shell_command_t shell_commands[] = {
	    { "abort", "Abort the program", abort_cmd },
#if WDT_HAS_STOP
	    { "wdt_stop", "Stop WDT", wdt_stop_cmd },
#endif
	    { NULL, NULL, NULL }
};
#endif
