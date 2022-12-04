/*
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

#include "sensors.h"

// TODO add LM75 (for lora-e5-dev)


#if BMX280 == 1
#include "fmt.h"
#include "bmx280.h"
#include "bmx280_params.h"
#define FLAG_ERROR_BMX280           0x01

static bmx280_t bmx280_dev;
static bool bmx280_error;
static int16_t temperature = 0;
static uint32_t pressure = 0;
#if defined(MODULE_BME280_SPI) || defined(MODULE_BME280_I2C)
static uint16_t humidity = 0;
#endif

static int init_bmx280(void)
{
    DEBUG("[bmx280] Initializing\n");
    switch (bmx280_init(&bmx280_dev, &bmx280_params[0]))
    {
    case BMX280_ERR_BUS:
        DEBUG("[bmx280] ERROR : Something went wrong when using the I2C bus\n");
        return 1;
    case BMX280_ERR_NODEV:
        DEBUG("[[bmx280] ERROR : Unable to communicate with any BMX280 device\n");
        return 1;
    default:
        /* all good -> do nothing */
        break;
    }

    DEBUG("[bmx280] Initialization successful\n");
    DEBUG("[bmx280] Calibration Data\n");
    DEBUG("dig_T1: %u\n", bmx280_dev.calibration.dig_T1);
    DEBUG("dig_T2: %i\n", bmx280_dev.calibration.dig_T2);
    DEBUG("dig_T3: %i\n", bmx280_dev.calibration.dig_T3);

    DEBUG("dig_P1: %u\n", bmx280_dev.calibration.dig_P1);
    DEBUG("dig_P2: %i\n", bmx280_dev.calibration.dig_P2);
    DEBUG("dig_P3: %i\n", bmx280_dev.calibration.dig_P3);
    DEBUG("dig_P4: %i\n", bmx280_dev.calibration.dig_P4);
    DEBUG("dig_P5: %i\n", bmx280_dev.calibration.dig_P5);
    DEBUG("dig_P6: %i\n", bmx280_dev.calibration.dig_P6);
    DEBUG("dig_P7: %i\n", bmx280_dev.calibration.dig_P7);
    DEBUG("dig_P8: %i\n", bmx280_dev.calibration.dig_P8);
    DEBUG("dig_P9: %i\n", bmx280_dev.calibration.dig_P9);

#if defined(MODULE_BME280_SPI) || defined(MODULE_BME280_I2C)
    DEBUG("dig_H1: %u\n", bmx280_dev.calibration.dig_H1);
    DEBUG("dig_H2: %i\n", bmx280_dev.calibration.dig_H2);
    DEBUG("dig_H3: %i\n", bmx280_dev.calibration.dig_H3);
    DEBUG("dig_H4: %i\n", bmx280_dev.calibration.dig_H4);
    DEBUG("dig_H5: %i\n", bmx280_dev.calibration.dig_H5);
    DEBUG("dig_H6: %i\n", bmx280_dev.calibration.dig_H6);
#endif

    return 0;
}

static int read_bmx280(void)
{
    /* read temperature, pressure [and humidity] values */
    temperature = bmx280_read_temperature(&bmx280_dev);
    pressure = bmx280_read_pressure(&bmx280_dev);
#if defined(MODULE_BME280_SPI) || defined(MODULE_BME280_I2C)
    humidity = bme280_read_humidity(&bmx280_dev);
#endif

    /* format values for printing */
    char str_temp[8];
    size_t len = fmt_s16_dfp(str_temp, temperature, -2);
    str_temp[len] = '\0';
#if defined(MODULE_BME280_SPI) || defined(MODULE_BME280_I2C)
    char str_hum[8];
    len = fmt_s16_dfp(str_hum, humidity, -2);
    str_hum[len] = '\0';
#endif

    /* print values to STDIO */
    DEBUG("[bmx280] temperature=%s°C", str_temp);
    DEBUG(" pressure=%" PRIu32 "Pa", pressure);
#if defined(MODULE_BME280_SPI) || defined(MODULE_BME280_I2C)
    DEBUG(" humidity=%s%%rH", str_hum);
#endif
    DEBUG("\n");
    return 0;
}
#endif

#if PMS7003 == 1
#include "pms7003_driver.h"
#define FLAG_ERROR_PMS7003          0x02

static struct pms7003Data pms7003_data;
static bool pms7003_error;
#endif


#if GPS == 1
#include "gps.h"
#define FLAG_ERROR_GPS           0x04

#endif

/* Declare globally the sensor device descriptor */
#if DS75LX == 1
#include "ds75lx.h"
#include "ds75lx_params.h"
static ds75lx_t ds75lx;
#define FLAG_ERROR_DS75LX           0x08
#endif

#if AT30TES75X == 1
#include "at30tse75x.h"
static at30tse75x_t at30tse75x;
#define FLAG_ERROR_AT30TES75X           0x10
#endif


/**
 * Initialize the endpoint's sensors
 */
uint8_t init_sensors(void) {

	uint8_t init_error_flags = 0x00; // For error flags

#if BMX280 == 1
    int ret = init_bmx280();
    bmx280_error = (ret!=0);
    init_error_flags = init_error_flags | FLAG_ERROR_BMX280;
#endif

#if PMS7003 == 1
    int ret2 = pms7003_init(false);
    pms7003_error = (ret2!=0);
    if(ret2==0){
        pms7003_measure(&pms7003_data);
        pms7003_print(&pms7003_data);

    } else {
        pms7003_error = (ret!=0);
        init_error_flags = init_error_flags | FLAG_ERROR_PMS7003;
    }
#endif

#if GPS == 1
    DEBUG("[gps] GPS is enabled (baudrate=%d)\n",STD_BAUDRATE);
#endif

#if DS75LX == 1
    DEBUG("[ds75lx] DS75LX sensor is enabled\n");

    int result = ds75lx_init(&ds75lx, &ds75lx_params[0]);
    if (result != DS75LX_OK)
    {
        DEBUG("[error] Failed to initialize DS75LX sensor\n");
        init_error_flags = init_error_flags | FLAG_ERROR_DS75LX;
    }
#endif

#if AT30TES75X == 1
    DEBUG("[at30tse75x] AT30TES75X sensor is enabled\n");

    int result = at30tse75x_init(&at30tse75x, PORT_A, AT30TSE75X_TEMP_ADDR);
    if (result != 0)
    {
        DEBUG("[error] Failed to initialize AT30TES75X sensor\n");
        init_error_flags = init_error_flags | FLAG_ERROR_AT30TES75X;
    }
#endif

	return init_error_flags;
}

/**
 *  Encode message data to the payload.
 *
 */
uint8_t encode_sensors(uint8_t *payload) {

	payload[0] = 0; // For error flags

	uint8_t i = 1;

#if BMX280 == 1
    if(!bmx280_error) {

        read_bmx280();

        // Encode temperature.
        memcpy(payload+i, &temperature, sizeof(int16_t));
        i+=sizeof(int16_t);

        // Encode pressure.
        uint16_t _pressure = pressure / 10;
        memcpy(payload+i, &_pressure, sizeof(uint16_t));
        i+=sizeof(uint16_t);

#if defined(MODULE_BME280_SPI) || defined(MODULE_BME280_I2C)
        // Encode humidity.
        memcpy(payload+i, &humidity, sizeof(uint16_t));
        i+=sizeof(uint16_t);
#endif
    }  else {
        payload[0] = payload[0] | FLAG_ERROR_BMX280;
    }

#endif

#if PMS7003 == 1
    if(!pms7003_error) {

        pms7003_measure(&pms7003_data);
        pms7003_print(&pms7003_data);
#ifdef PMS7003_OUTPUT_CSV
        // TODO: prefix CSV by timestamp
        pms7003_print_csv(&pms7003_data);
#endif

        memcpy(payload+i, &pms7003_data.pm1_0Standard, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.pm2_5Standard, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.pm10Standard, sizeof(uint16_t));
        i+=sizeof(uint16_t);


        memcpy(payload+i, &pms7003_data.pm1_0Atmospheric, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.pm2_5Atmospheric, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.pm10Atmospheric, sizeof(uint16_t));
        i+=sizeof(uint16_t);


        memcpy(payload+i, &pms7003_data.particuleGT0_3, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.particuleGT0_5, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.particuleGT1_0, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.particuleGT2_5, sizeof(uint16_t));
        i+=sizeof(uint16_t);

        memcpy(payload+i, &pms7003_data.particuleGT10, sizeof(uint16_t));
        i+=sizeof(uint16_t);
    }  else {
        payload[0] = payload[0] | FLAG_ERROR_PMS7003;
    }
#endif

#if DS75LX == 1
    if(!ds75lx_error) {
		int16_t temperature = 0;

		/* measure temperature */
		ds75lx_wakeup(&ds75lx);
		/* Get temperature in degrees celsius */
		ds75lx_read_temperature(&ds75lx, &temperature);
		ds75lx_shutdown(&ds75lx);
		DEBUG("[ds75lx] get temperature : temperature=%d\n",temperature);

		// Encode temperature.
		payload[i++] = (temperature >> 8) & 0xFF;
		payload[i++] = (temperature >> 0) & 0xFF;

		if(len < sizeof(int16_t) + (2*3)+ sizeof(int16_t)) {
			return sizeof(int16_t);
		}
    }
#endif

#if AT30TES75X == 1
    if(!at30tse75x_error) {
		int16_t temperature = 0;

		/* measure temperature */
		//at30tse75x_wakeup(&at30tse75x);
		/* Get temperature in degrees celsius */
		float ftemp;
		at30tse75x_get_temperature(&at30tse75x, &ftemp);
		temperature = (int16_t)(ftemp * 100);
		//at30tse75x_shutdown(&at30tse75x);
		DEBUG("[at30tse75x] get temperature : temperature=%d\n",temperature);

		// Encode temperature.
		payload[i++] = (temperature >> 8) & 0xFF;
		payload[i++] = (temperature >> 0) & 0xFF;

		if(len < sizeof(int16_t) + (2*3)+ sizeof(int16_t)) {
			return sizeof(int16_t);
		}
    }
#endif

#if GPS == 1
	int32_t lat = 0;
	int32_t lon = 0;
	int16_t alt = 0;

	gps_get_binary(&lat, &lon, &alt);
    DEBUG("[gps] get position : lat=%ld, lon=%ld, alt=%d\n",lat,lon,alt);

    // Encode latitude (on 24 bits).
	payload[i++] = ((uint32_t)lat >> 16) & 0xFF;
	payload[i++] = ((uint32_t)lat >> 8)  & 0xFF;
	payload[i++] = ((uint32_t)lat >> 0)  & 0xFF;

    // Encode longitude (on 24 bits).
	payload[i++] = ((uint32_t)lon >> 16) & 0xFF;
	payload[i++] = ((uint32_t)lon >> 8)  & 0xFF;
	payload[i++] = ((uint32_t)lon >> 0)  & 0xFF;

    // Encode altitude (on 16 bits);
	payload[i++] = ((int16_t)alt >> 8) & 0xFF;
	payload[i++] = ((int16_t)alt >> 0) & 0xFF;

#endif

	return i;
}
