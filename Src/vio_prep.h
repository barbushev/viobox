/*
 * vio_prep.h
 *
 *  Created on: Mar 19, 2018
 *      Author: Ted
 */

#ifndef VIO_PREP_H_
#define VIO_PREP_H_

#include "viobox.h"
#include "stdint.h"

vio_status_t vio_prep_init();

vio_status_t vio_get_prep_min_time_us(char *buf, uint8_t len);
vio_status_t vio_get_prep_max_time_us(char *buf, uint8_t len);

vio_status_t vio_get_prep_timeus(char *buf, uint8_t len);
vio_status_t vio_get_prep_isrunning(char *buf, uint8_t len);
vio_status_t vio_get_prep_notify(char *buf, uint8_t len);

vio_status_t vio_set_prep_timeus(const void *);
vio_status_t vio_set_prep_isrunning(const void *);
vio_status_t vio_set_prep_notify(const void *);

#endif /* VIO_PREP_H_ */
