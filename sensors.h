/*
 * Copyright (C) 2020-2022 LIG Université Grenoble Alpes
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * @author Didier DONSEZ
 */

#ifndef SENSORS_H_
#define SENSORS_H_

#include <stdint.h>

/**
 * Initialize the endpoint's sensors
 */
uint8_t init_sensors(void);

/**
 *  Encode message data to the payload.
 *
 */
uint8_t encode_sensors(uint8_t *payload);


#endif /* SENSORS_H_ */
