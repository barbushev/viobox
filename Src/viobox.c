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
static vio_status_t vio_get_prep_timeus(char *buf, uint8_t len);
static vio_status_t vio_get_prep_isrunning(char *buf, uint8_t len);

static vio_status_t vio_set_ssr_state(vio_ssr_state_t newState);

//this a jump table
vio_status_t(*vio_get[])(char *buf, uint8_t len) =
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
		[VIO_CMD_GET_PREP_TIMEUS] = vio_get_prep_timeus,
		[VIO_CMD_GET_PREP_ISRUNNING] = vio_get_prep_isrunning
};



static vio_status_t setExposeReqState(vio_expreq_state_t);
static vio_status_t getExposeReqState(vio_expreq_state_t *);

static vio_status_t setExposeReqFrequency(uint32_t);
static vio_status_t getExposeReqFrequency(uint32_t *);

static vio_status_t varPwmAdd(uint32_t);
static vio_status_t varPwmClear(void);


static vio_status_t isPeriodInRange(const uint32_t *);


static void sendData(const char *buf, uint8_t len);
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
};

static vio_prepare_t prep =
{
		.timeUs = 0,
		.isRunning = false,
};

static vio_exposeok_t expOk =
{
		.count = 0,
		.notify = false,
};

/*
	vio_status_t(*Stop)(void);
	vio_status_t(*Start)(void);
*/

//prep - output
//expose request - output

//expose ok      - input
//PWR - output


void vio_ProcessIncomingData(const char *inBuf, uint16_t len)
{
	vio_status_t result = VIO_STATUS_OK;



	/*
	uint32_t data[2]; //1 command + up to 1 parameters
	char *token;
	token = strtok ((char *)Buf, " ");
	for (uint8_t i = 0; i < 5; i++)
	{
		if (token == NULL) break;
		data[i] = atoi(token);
		token = strtok (NULL, " ");
	}
	*/

	char respParam[48] = "";

	char *pLeftover;
	int32_t cmd = strtol(inBuf, &pLeftover, 10);
	//if (pLeftover[0] == VIO_COMM_DELIMETER)

	if ((cmd > VIO_CMD_GET_BLOCK_START) && (cmd < VIO_CMD_GET_BLOCK_END))
		vio_get[cmd](respParam, 48);

/*
	switch(cmd)
	{
		case VIO_CMD_GET_FW_VER:
		{
			//USB_SEND("%d.%d.%d\n", VIO_MAJOR_VERSION, VIO_MINOR_VERSION, VIO_PATCH_VERSION);
			snprintf(respParam, sizeof(respParam), " %d.%d.%d", VIO_MAJOR_VERSION, VIO_MINOR_VERSION, VIO_PATCH_VERSION);
			break;
		}

		case VIO_CMD_GET_MIN_PERIOD_US:
			snprintf(respParam, sizeof(respParam), " %d", VIO_MIN_PERIOD_US);
			break;

		case VIO_CMD_GET_MAX_PERIOD_US:
			snprintf(respParam, sizeof(respParam), " %d", VIO_MAX_PERIOD_US);
			break;

		case VIO_CMD_GET_MAX_SEQUENCE_LENGTH:
			snprintf(respParam, sizeof(respParam), " %d", VIO_MAX_SEQUENCE_LENGTH);
			break;

		case VIO_CMD_SET_EXPREQ_FREQUENCY:
		{
			result = setExposeReqFrequency(strtol(pLeftover, NULL, 10));
			break;
		}

		case VIO_CMD_GET_EXPREQ_FREQUENCY:
		{
			uint32_t periodUs;
			result = getExposeReqFrequency(&periodUs);
			if (result == VIO_STATUS_OK)
				snprintf(respParam, sizeof(respParam), " %lu", periodUs);
			break;
		}

		case VIO_CMD_SET_EXPREQ_STATE:
		{
			result = setExposeReqState(strtol(pLeftover, NULL, 10));
			break;
		}

		case VIO_CMD_GET_EXPREQ_STATE:
		{
			vio_expreq_state_t curState;
			result = getExposeReqState(&curState);
			if (result == VIO_STATUS_OK)
				snprintf(respParam, sizeof(respParam), " %d", curState);
			break;
		}

		case VIO_CMD_SET_SSR_STATE:
		{
			//result = setSsrState(strtol(pLeftover, NULL, 10));
		//	result = vio_set_ssr_state();
			break;
		}

		case VIO_CMD_GET_SSR_STATE:
		{
			//bvio_ssr_state_t curState;
			//vio_set_ssr_state()
			//vio_get_ssr_state
			//result = getSsrState(&curState);
			if (result == VIO_STATUS_OK)
				snprintf(respParam, sizeof(respParam), " %d", curState);
			break;
		}

		case VIO_CMD_SET_EXPREQ_VARPWM_ADD:
		{
			result = varPwmAdd(strtol(pLeftover, NULL, 10));
			break;
		}

		case VIO_CMD_SET_EXPREQ_VARPWM_EMPTY:
		{
			result = varPwmClear();
			break;
		}


		case VIO_CMD_SET_EXPREQ_ONE_PULSE_LENGTH:

		case VIO_CMD_SET_EXPREQ_FIXPWM_PWIDTH:
		case VIO_CMD_SET_EXPREQ_VARPWM_LOOPING:  /// Takes a parameter of bool (0 or 1). Enables or disables looping.
		case VIO_CMD_SET_EXPREQ_VARPWM_WAITFOREXTSYNC:

			break;

		default:
		{
			result = VIO_STATUS_BAD_COMMAND;
			break;
		}
	}
*/
	char resp[64];
	snprintf(resp, sizeof(resp), "%ld %d%s\n", cmd, result, respParam);
    sendData(resp, strlen(resp));
}

static vio_status_t varPwmAdd(uint32_t pulseWidth)
{
	if((expReq.VarPwm.count >= VIO_MAX_SEQUENCE_LENGTH))
		return VIO_STATUS_SEQUENCE_QUEUE_FULL;

	if(isPeriodInRange(&pulseWidth) != VIO_STATUS_OK)
		return isPeriodInRange(&pulseWidth);

	expReq.VarPwm.elements[expReq.VarPwm.count++] = pulseWidth;
	return VIO_STATUS_OK;
}

static vio_status_t varPwmClear(void)
{
	if(expReq.State == VIO_EXPREQ_VARIABLE_PWM)
		return VIO_STATUS_SEQUENCE_IS_RUNNING;

	memset(expReq.VarPwm.elements, 0, expReq.VarPwm.count);
	expReq.VarPwm.count = 0;
	return VIO_STATUS_OK;
}


static vio_status_t setExposeReqFrequency(uint32_t periodUs)
{
	if (isPeriodInRange(&periodUs) != VIO_STATUS_OK)
		return isPeriodInRange(&periodUs);

	expReq.fPeriodUs = periodUs;

	return VIO_STATUS_OK;
}

static vio_status_t getExposeReqFrequency(uint32_t *periodUs)
{
	*periodUs = expReq.fPeriodUs;
	return VIO_STATUS_OK;
}


static vio_status_t isPeriodInRange(const uint32_t *fPeriodUs)
{
	if(((*fPeriodUs < VIO_MIN_PERIOD_US) && (*fPeriodUs > VIO_MAX_PERIOD_US)))
		return VIO_STATUS_OUT_OF_RANGE;

	return VIO_STATUS_OK;
}

static vio_status_t setExposeReqState(vio_expreq_state_t newState)
{
	switch(newState)
	{
		case VIO_EXPREQ_IDLE:
		case VIO_EXPREQ_ONE_PULSE:
		case VIO_EXPREQ_FIXED_PWM:
		case VIO_EXPREQ_VARIABLE_PWM:
			expReq.State = newState;
			break;
		default:
			return VIO_STATUS_GENERIC_ERROR;
	}

	return VIO_STATUS_OK;
}

static vio_status_t getExposeReqState(vio_expreq_state_t *curState)
{
	*curState = expReq.State;
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_fw_ver(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d.%d.%d", VIO_MAJOR_VERSION, VIO_MINOR_VERSION, VIO_PATCH_VERSION);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_min_period_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", VIO_MIN_PERIOD_US);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_max_period_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", VIO_MAX_PERIOD_US);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_max_sequence_length(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", VIO_MAX_SEQUENCE_LENGTH);
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

static vio_status_t vio_set_ssr_state(vio_ssr_state_t newState)
{
	if ((newState != VIO_SSR_OPEN) && (newState != VIO_SSR_CLOSED))
			return VIO_STATUS_BAD_PARAMETER;

	HAL_GPIO_WritePin(POWER_CONTROL_OUT_GPIO_Port, POWER_CONTROL_OUT_Pin, newState);
	return VIO_STATUS_OK;
}


/*
VIO_CMD_SET_SSR_STATE,		/// Set the state of Solid State Relay. Requires a vio_ssr_state_t parameter.
	VIO_CMD_SET_EXPREQ_ONE_PULSE_LENGTH,
	VIO_CMD_SET_EXPREQ_STATE,	/// Set the state of Expose Request. Requires a vio_expreq_state_t parameter.
	VIO_CMD_SET_EXPREQ_FREQUENCY,
	VIO_CMD_SET_EXPREQ_FIXPWM_PWIDTH,
	VIO_CMD_SET_EXPREQ_VARPWM_LOOPING,  /// Takes a parameter of bool (0 or 1). Enables or disables looping.
	VIO_CMD_SET_EXPREQ_VARPWM_WAITFOREXTSYNC,
	VIO_CMD_SET_EXPREQ_VARPWM_EMPTY,           /// Clear all vio_expreq_varpwm_t.elements
	VIO_CMD_SET_EXPREQ_VARPWM_ADD,			   /// Takes a parameter pulseWidthUs. Adds it to the end of the queue.
	VIO_CMD_SET_PREP_TIMEUS,
	VIO_CMD_SET_PREP_ISRUNNING,
*/


static void sendData(const char *buf, uint8_t len)
{
	CDC_Transmit_FS(buf, len);
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
