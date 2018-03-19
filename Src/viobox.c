/*
 * viobox.c
 *	Varex IO Box
 *
 *  Created on: Mar 13, 2018
 *      Author: Ted Barbushev
 */
#include "viobox.h"
#include "stddef.h"
#include "stdarg.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "usbd_cdc_if.h"
#include "usbd_desc.h"

#include "exp_req_ctrl.h"

#define __HAL_TIM_GET_PRESCALER(__HANDLE__)       ((__HANDLE__)->Instance->PSC)

static vio_status_t vio_get_fw_ver(char *buf, uint8_t len);
static vio_status_t vio_get_min_period_us(char *buf, uint8_t len);
static vio_status_t vio_get_max_period_us(char *buf, uint8_t len);
static vio_status_t vio_get_max_sequence_length(char *buf, uint8_t len);
static vio_status_t vio_get_ssr_state(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_state(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_frequency(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_one_pulse_length(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_fixpwm_pwidth(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_varpwm_count(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_varpwm_position(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_varpwm_looping(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_varpwm_numloops(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_varpwm_waitforextsync(char *buf, uint8_t len);
static vio_status_t vio_get_expreq_varpwm_notify(char *buf, uint8_t len);
static vio_status_t vio_get_prep_timeus(char *buf, uint8_t len);
static vio_status_t vio_get_prep_isrunning(char *buf, uint8_t len);
static vio_status_t vio_get_prep_notify(char *buf, uint8_t len);
static vio_status_t vio_get_expok_count(char *buf, uint8_t len);
static vio_status_t vio_get_expok_notify(char *buf, uint8_t len);
static vio_status_t vio_get_serial_number(char *buf, uint8_t len);

static vio_status_t vio_set_ssr_state(const void *);
static vio_status_t vio_set_expreq_one_pulse_length(const void *);
static vio_status_t vio_set_expreq_state(const void *);
static vio_status_t vio_set_expreq_frequency(const void *);
static vio_status_t vio_set_expreq_fixpwm_pwidth(const void *);
static vio_status_t vio_set_expreq_varpwm_looping(const void *);
static vio_status_t vio_set_expreq_varpwm_waitforextsync(const void *);
static vio_status_t vio_set_expreq_varpwm_empty();
static vio_status_t vio_set_expreq_varpwm_add(const void *);
static vio_status_t vio_set_expreq_varpwm_notify(const void *);
static vio_status_t vio_set_prep_timeus(const void *);
static vio_status_t vio_set_prep_isrunning(const void *);
static vio_status_t vio_set_prep_notify(const void *);
static vio_status_t vio_set_expok_count_zero();
static vio_status_t vio_set_expok_notify(const void *);
static vio_status_t vio_set_serial_number(const void *);

static vio_status_t vio_sys_reboot();
//static vio_status_t vio_sys_dfu();

//this a jump table
static vio_status_t(*vio_get[])(char *, uint8_t) =
{
		[VIO_CMD_GET_FW_VER] = vio_get_fw_ver,
		[VIO_CMD_GET_MIN_PERIOD_US] = vio_get_min_period_us,
		[VIO_CMD_GET_MAX_PERIOD_US] = vio_get_max_period_us,
		[VIO_CMD_GET_MAX_SEQUENCE_LENGTH] = vio_get_max_sequence_length,
		[VIO_CMD_GET_SSR_STATE] = vio_get_ssr_state,
		[VIO_CMD_GET_EXPREQ_STATE] = vio_get_expreq_state,
		[VIO_CMD_GET_EXPREQ_FREQUENCY] = vio_get_expreq_frequency,
		[VIO_CMD_GET_EXPREQ_ONE_PULSE_LENGTH] = vio_get_expreq_one_pulse_length,
		[VIO_CMD_GET_EXPREQ_FIXPWM_PWIDTH] = vio_get_expreq_fixpwm_pwidth,
		[VIO_CMD_GET_EXPREQ_VARPWM_COUNT] = vio_get_expreq_varpwm_count,
		[VIO_CMD_GET_EXPREQ_VARPWM_POSITION] = vio_get_expreq_varpwm_position,
		[VIO_CMD_GET_EXPREQ_VARPWM_LOOPING] = vio_get_expreq_varpwm_looping,
		[VIO_CMD_GET_EXPREQ_VARPWM_NUMLOOPS] = vio_get_expreq_varpwm_numloops,
		[VIO_CMD_GET_EXPREQ_VARPWM_WAITFOREXTSYNC] = vio_get_expreq_varpwm_waitforextsync,
		[VIO_CMD_GET_EXPREQ_VARPWM_NOTIFY] = vio_get_expreq_varpwm_notify,
		[VIO_CMD_GET_PREP_TIMEUS] = vio_get_prep_timeus,
		[VIO_CMD_GET_PREP_ISRUNNING] = vio_get_prep_isrunning,
		[VIO_CMD_GET_PREP_NOTIFY] = vio_get_prep_notify,
		[VIO_CMD_GET_EXPOK_COUNT] = vio_get_expok_count,
		[VIO_CMD_GET_EXPOK_NOTIFY] = vio_get_expok_notify,
		[VIO_CMD_GET_SERIAL_NUMBER] = vio_get_serial_number,
};

static vio_status_t(*vio_set[])(const void *) =
{
	[VIO_CMD_SET_SSR_STATE] = vio_set_ssr_state,							 	/// Set the state of Solid State Relay. Requires a vio_ssr_state_t parameter.
	[VIO_CMD_SET_EXPREQ_ONE_PULSE_LENGTH] = vio_set_expreq_one_pulse_length,
	[VIO_CMD_SET_EXPREQ_STATE] = vio_set_expreq_state,							/// Set the state of Expose Request. Requires a vio_expreq_state_t parameter.
	[VIO_CMD_SET_EXPREQ_FREQUENCY] = vio_set_expreq_frequency,
	[VIO_CMD_SET_EXPREQ_FIXPWM_PWIDTH] = vio_set_expreq_fixpwm_pwidth,
	[VIO_CMD_SET_EXPREQ_VARPWM_LOOPING] = vio_set_expreq_varpwm_looping,  		/// Takes a parameter of bool (0 or 1). Enables or disables looping.
	[VIO_CMD_SET_EXPREQ_VARPWM_WAITFOREXTSYNC] = vio_set_expreq_varpwm_waitforextsync,
	[VIO_CMD_SET_EXPREQ_VARPWM_EMPTY] = vio_set_expreq_varpwm_empty,            /// Clear all vio_expreq_varpwm_t.elements
	[VIO_CMD_SET_EXPREQ_VARPWM_ADD] = vio_set_expreq_varpwm_add,			    /// Takes a parameter pulseWidthUs. Adds it to the end of the queue.
	[VIO_CMD_SET_EXPREQ_VARPWM_NOTIFY] = vio_set_expreq_varpwm_notify,
	[VIO_CMD_SET_PREP_TIMEUS] = vio_set_prep_timeus,
	[VIO_CMD_SET_PREP_ISRUNNING] = vio_set_prep_isrunning,
	[VIO_CMD_SET_PREP_NOTIFY] = vio_set_prep_notify,
	[VIO_CMD_SET_EXPOK_COUNT_ZERO] = vio_set_expok_count_zero,
	[VIO_CMD_SET_EXPOK_NOTIFY] = vio_set_expok_notify,
	[VIO_CMD_SET_SERIAL_NUMBER] = vio_set_serial_number,
};

static vio_status_t(*vio_sys[])() =
{
	[VIO_CMD_SYS_REBOOT] = vio_sys_reboot,
};

static vio_status_t vio_is_period_in_range(const uint32_t *);
static void vio_send_data(const char *buf, uint8_t len);
static bool vio_is_valid_bool(const bool *);

//static void USB_SEND(const char *string, ...);

static vio_expose_request_t expReq =
{
	.State = VIO_EXPREQ_IDLE,
	.fPeriodUs = 0,
	.OnePulse.lengthUs = 0,
	.FixedPwm.widthUs = 0,
	.VarPwm.count = 0,
	.VarPwm.position = 0,
	.VarPwm.enableLooping = false,
	.VarPwm.numLoops = 0,
	.VarPwm.waitForExtSync = false,
	.VarPwm.notify = false,
};

static vio_prepare_t prep =
{
		.timeUs = 0,
		.isRunning = false,
		.notify = false,
};

static vio_exposeok_t expOk =
{
		.count = 0,
		.notify = false,
};

void vio_init()
{
	exp_req_init();
}

void vio_recv_data(const char *inBuf, uint16_t len)
{
	vio_status_t result = VIO_STATUS_BAD_COMMAND;
	char respParam[48] = "";
	char *pLeftover = NULL;

	uint32_t cmd = strtoul(inBuf, &pLeftover, 10);

	if((cmd > VIO_CMD_GET_BLOCK_START) && (cmd < VIO_CMD_GET_BLOCK_END)) // Check if it is a GET command
	{
		result = vio_get[cmd](respParam, sizeof(respParam));
	}
	else if((cmd > VIO_CMD_SET_BLOCK_START) && (cmd < VIO_CMD_SET_BLOCK_END))  //Check if it is a SET command
	{
		result = VIO_STATUS_BAD_PARAMETER;  //assume a bad parameter then check
		if (pLeftover[0] == VIO_COMM_DELIMETER)
		{
			char *tail = NULL;
			uint32_t prm = strtoul(pLeftover, &tail, 10);
			if(tail[0] == VIO_COMM_TERMINATOR)
			{
				result = vio_set[cmd](&prm);
				snprintf(respParam, sizeof(respParam), "%lu", prm);
			}
		}
	}
	else if((cmd > VIO_CMD_GET_BLOCK_END) && (cmd < VIO_CMD_SET_BLOCK_START))  //Check if it is a System command
	{
		result = vio_sys[cmd]();
	}

	char resp[64];
	snprintf(resp, sizeof(resp), "%lu%c%d%c%s\n", cmd, VIO_COMM_DELIMETER, result, VIO_COMM_DELIMETER, respParam);
	vio_send_data(resp, strlen(resp));
}

static bool vio_is_valid_bool(const bool *value)
{
	return ((*value == true) || (*value == false));
}


static vio_status_t vio_is_period_in_range(const uint32_t *fPeriodUs)
{
	if(((*fPeriodUs < VIO_MIN_PERIOD_US) || (*fPeriodUs > VIO_MAX_PERIOD_US)))
		return VIO_STATUS_OUT_OF_RANGE;

	return VIO_STATUS_OK;
}

static vio_status_t vio_get_fw_ver(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d.%d.%d", VIO_MAJOR_VERSION, VIO_MINOR_VERSION, VIO_PATCH_VERSION);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_min_period_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_MIN_PERIOD_US);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_max_period_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_MAX_PERIOD_US);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_max_sequence_length(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", VIO_MAX_VARPWM_SEQUENCE_LENGTH);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_ssr_state(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", HAL_GPIO_ReadPin(POWER_CONTROL_OUT_GPIO_Port, POWER_CONTROL_OUT_Pin));
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_state(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.State);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_frequency(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.fPeriodUs);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_one_pulse_length(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.OnePulse.lengthUs);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_fixpwm_pwidth(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.FixedPwm.widthUs);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_varpwm_count(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.count);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_varpwm_position(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.position);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_varpwm_looping(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.enableLooping);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_varpwm_numloops(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.VarPwm.numLoops);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_varpwm_waitforextsync(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.waitForExtSync);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expreq_varpwm_notify(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.notify);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_prep_timeus(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", prep.timeUs);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_prep_isrunning(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", prep.isRunning);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_prep_notify(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", prep.notify);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expok_count(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expOk.count);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_expok_notify(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expOk.notify);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_serial_number(char *buf, uint8_t len)
{
	buf = (char *)FS_Desc.GetSerialStrDescriptor(USBD_SPEED_FULL, (uint16_t *)&len);
	return VIO_STATUS_OK;
}

//////////////////////////SET FUNCTIONS/////////////////////////////////////////////////


static vio_status_t vio_set_ssr_state(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	switch(*(vio_ssr_state_t *)newValue)
	{
		case VIO_SSR_OPEN:
		case VIO_SSR_CLOSED:
			HAL_GPIO_WritePin(POWER_CONTROL_OUT_GPIO_Port, POWER_CONTROL_OUT_Pin, *(vio_ssr_state_t *)newValue);
			break;
		default:
			result = VIO_STATUS_BAD_PARAMETER;
	}

	return result;
}

static vio_status_t vio_set_expreq_one_pulse_length(const void *newValue)
{
	vio_status_t result = vio_is_period_in_range((uint32_t *)newValue);
	if (result == VIO_STATUS_OK)
		expReq.OnePulse.lengthUs = *(uint32_t *)newValue;

	return result;
}

static vio_status_t vio_set_expreq_state(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;

	switch(*(vio_expreq_state_t *)newValue)
	{
		case VIO_EXPREQ_IDLE:
			HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
			break;

		case VIO_EXPREQ_ONE_PULSE:
			break;

		case VIO_EXPREQ_FIXED_PWM:
			HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
			break;

		case VIO_EXPREQ_VARIABLE_PWM:
			break;

		default:
			result = VIO_STATUS_BAD_PARAMETER;
	}

	return result;
}

static vio_status_t vio_set_expreq_frequency(const void *newValue)
{
	vio_status_t result = vio_is_period_in_range((uint32_t *)newValue);
	if(result == VIO_STATUS_OK)
	{
		expReq.fPeriodUs = *(uint32_t *)newValue;
		static const uint32_t TIMER_RESOLUTION = 65536;

		uint32_t cycles = (HAL_RCC_GetPCLK2Freq() / (1000000.0 / *(uint32_t *)newValue));  //1000000 / microseconds - converting back to Hz
		uint32_t prescaler = (cycles / TIMER_RESOLUTION) - 1;
		if(cycles % TIMER_RESOLUTION != 0) prescaler++;  //if there was a left over, increment by 1
		uint32_t period = (float)cycles / (float)(prescaler + 1); //perform division as float and add 1 back to prescaler.

		__HAL_TIM_SET_PRESCALER(&htim1, prescaler);
		__HAL_TIM_SET_AUTORELOAD(&htim1, period);

		char buf[64];   //the stuff below is used for debugging. Will remove later.
		snprintf(buf, sizeof(buf), "cyl: %lu, scl: %lu, per: %lu\n", cycles, prescaler, period);
		CDC_Transmit_FS(buf, strlen(buf));
	}

	return result;
}

static vio_status_t vio_set_expreq_fixpwm_pwidth(const void *newValue)
{
	vio_status_t result = vio_is_period_in_range((uint32_t *)newValue);
	if (result == VIO_STATUS_OK)
	{
		uint32_t cycles = (HAL_RCC_GetPCLK2Freq() / (1000000.0 / *(uint32_t *)newValue));
		uint32_t period = (float)cycles / (float)(__HAL_TIM_GET_PRESCALER(&htim1) + 1);

		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, period);
		expReq.FixedPwm.widthUs = *(uint32_t *)newValue;
	}

	return result;
}

static vio_status_t vio_set_expreq_varpwm_looping(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		expReq.VarPwm.enableLooping = *(bool *)newValue;

	return result;
}

static vio_status_t vio_set_expreq_varpwm_waitforextsync(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		expReq.VarPwm.waitForExtSync = *(bool *)newValue;

	return result;
}

static vio_status_t vio_set_expreq_varpwm_empty()
{
	vio_status_t result = VIO_STATUS_OK;
	if (expReq.State == VIO_EXPREQ_VARIABLE_PWM)
		result = VIO_STATUS_VARPWM_IS_RUNNING;
	else
	{
		memset(expReq.VarPwm.elements, 0, VIO_MAX_VARPWM_SEQUENCE_LENGTH);
		expReq.VarPwm.count = 0;
	}

	return result;
}

static vio_status_t vio_set_expreq_varpwm_add(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if((expReq.VarPwm.count >= VIO_MAX_VARPWM_SEQUENCE_LENGTH))
	{
		result = VIO_STATUS_SEQUENCE_QUEUE_FULL;
		goto error;
	}

	if(expReq.State == VIO_EXPREQ_VARIABLE_PWM)
	{
		result = VIO_STATUS_VARPWM_IS_RUNNING;
		goto error;
	}

	result = vio_is_period_in_range((uint32_t *)newValue);
	if(result == VIO_STATUS_OK)
		expReq.VarPwm.elements[expReq.VarPwm.count++] = *(uint16_t *)newValue;

	error:
	return result;
}

static vio_status_t vio_set_expreq_varpwm_notify(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		expReq.VarPwm.notify = *(bool *)newValue;

	return result;
}

static vio_status_t	vio_set_prep_timeus(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(prep.isRunning == true)
		result = VIO_STATUS_PREP_ALREADY_RUNNING;
	else
		prep.timeUs = *(uint32_t *)newValue;

	return result;
}

static vio_status_t vio_set_prep_isrunning(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(prep.timeUs <= 0)
		result = VIO_STATUS_PREP_NOT_CONFIGURED;
	else if((prep.isRunning == true) && *(bool *)newValue == true)
		result = VIO_STATUS_PREP_ALREADY_RUNNING;
	else
	{
		prep.isRunning = *(bool *)newValue;

	}

	return result;
}

static vio_status_t vio_set_prep_notify(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		prep.notify = *(bool *)newValue;

	return result;
}

static vio_status_t vio_set_expok_count_zero()
{
	expOk.count = 0;
	return VIO_STATUS_OK;
}

static vio_status_t vio_set_expok_notify(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		expOk.notify = *(bool *)newValue;

	return result;
}

static vio_status_t vio_set_serial_number(const void *newValue)
{
	return VIO_STATUS_OK;
}

static vio_status_t vio_sys_reboot()
{
	char msg[16]; //have to send a response here as this will not return to vio_rcv_data
    snprintf(msg, sizeof(msg), "%d%c%d%c", VIO_CMD_SYS_REBOOT, VIO_COMM_DELIMETER, VIO_STATUS_OK, VIO_COMM_TERMINATOR);
    vio_send_data(msg, strlen(msg));
    HAL_Delay(100);

    NVIC_SystemReset();
	return VIO_STATUS_OK;
}

static void vio_send_data(const char *buf, uint8_t len)
{
	CDC_Transmit_FS(buf, len);
}


/// @brief EXTI line detection callbacks
/// @param GPIO_Pin: Specifies the pins connected EXTI line
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == EXPOSE_OK_IN_Pin)
  {
	  expOk.count++;
	  if(expOk.notify)
	  {
		  char msg[16];
		  snprintf(msg, sizeof(msg), "%d%c%lu%c", VIO_STATUS_EXPOSEOK_EVENT, VIO_COMM_DELIMETER, expOk.count, VIO_COMM_TERMINATOR);
		  vio_send_data(msg, strlen(msg));
	  }
  }
}

void calc()
{
	// Tim1 is clocked for APB2 which is currently configured to run at the same speed as the CPU = 72 MHz
	// APB2 clock may be divided using TIM_CLOCKDIVISION_DIVx where x values may be are APB2/1, APB2/2 or APB2/4
	// After that, the clock is scaled using Prescaler. This value should be such that the period has the highest value without overflowing.
	// Note that prescaler is value is always (- 1) since it starts at 0. Eg. 0 is actually a prescale of 1. 3 is acutally a prescale of 4...
	// Example for 10 Hz PWM. 10 Hz = 100000 us. microsec = 100000

	// Calculate cycles
    // Without using the divider, 72 000 000 / (1000000 / microsec) = 72 000 000 / 10 = 7 200 000

	// Calculate prescaler
	// prescaler = cycles /  (16bit timer) = 7 200 000 / 65536 = 109.86328125.
	// Since there is a leftover, round up so it will not overflow and then subtract 1.
	// prescaler = 109.86328125 = round up to 110 - 1 = 109.

	// Calculate period
	// period = cycles / prescaler = 7 200 000 / 110 = 65454.54

	//__HAL_TIM_SET_COUNTER  // may use this to reset to 0?

	//_HAL_TIM_SET_PRESCALER
	//__HAL_TIM_SET_AUTORELOAD(&htim1, *(uint32_t *)newValue);

	//__HAL_TIM_ENABLE

	/*
	static const uint32_t TIMER_RESOLUTION = 65536;

	uint32_t microseconds = 100;

	uint32_t cycles = (HAL_RCC_GetPCLK2Freq() / (1000000.0 / microseconds));  //1000000 / microseconds - converting back to Hz
	uint32_t prescaler = (cycles / TIMER_RESOLUTION) - 1;
	if(cycles % TIMER_RESOLUTION != 0) prescaler++;  //if there was a left over, increment by 1

	uint32_t period = cycles / prescaler;
	*/
}

/*
static void USB_SEND(const char *string, ...)
{
	uint8_t bufSize = 64;
	char buf[bufSize];

	va_list args;
	va_start(args, string);
	uint8_t length = vsnprintf(buf, bufSize, string, args);
	va_end(args);

	CDC_Transmit_FS(buf, length);
}
*/
