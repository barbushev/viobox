/*
 * exp_req_ctrl.h
 *
 *  Created on: Mar 18, 2018
 *      Author: Ted
 */

#ifndef VIO_EXP_REQ_H_
#define VIO_EXP_REQ_H_

#include "viobox.h"
#include "stdint.h"

vio_status_t vio_exp_req_init();
vio_status_t vio_expreq_process();

vio_status_t vio_get_expreq_max_sequence_length(char *buf, uint8_t len);
vio_status_t vio_get_expreq_min_period_us(char *buf, uint8_t len);
vio_status_t vio_get_expreq_max_period_us(char *buf, uint8_t len);
vio_status_t vio_get_expreq_state(char *buf, uint8_t len);
vio_status_t vio_get_expreq_frequency(char *buf, uint8_t len);
vio_status_t vio_get_expreq_one_pulse_length(char *buf, uint8_t len);
vio_status_t vio_get_expreq_fixpwm_pwidth(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_count(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_position(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_looping(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_numloops(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_waitforextsync(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_list(char *buf, uint8_t len);
vio_status_t vio_get_expreq_varpwm_notify(char *buf, uint8_t len);

vio_status_t vio_set_expreq_one_pulse_length(const void *);
vio_status_t vio_set_expreq_state(const void *newValue);
vio_status_t vio_set_expreq_frequency(const void *newValue);
vio_status_t vio_set_expreq_fixpwm_pwidth(const void *newValue);
vio_status_t vio_set_expreq_varpwm_looping(const void *);
vio_status_t vio_set_expreq_varpwm_waitforextsync(const void *);
vio_status_t vio_set_expreq_varpwm_empty();
vio_status_t vio_set_expreq_varpwm_add(const void *);
vio_status_t vio_set_expreq_varpwm_notify(const void *);

void vio_expreq_irq_callback();

#endif /* VIO_EXP_REQ_H_ */
