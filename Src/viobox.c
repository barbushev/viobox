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

static vio_status_t setExposeReqState(vio_expreq_state_t);
static vio_status_t getExposeReqState(vio_expreq_state_t *);

static vio_status_t setExposeReqFrequency(uint32_t);
static vio_status_t getExposeReqFrequency(uint32_t *);

static vio_status_t varPwmAdd(uint32_t);
static vio_status_t varPwmClear(void);
static vio_status_t varPwmList(void);

static vio_status_t isPeriodInRange(const uint32_t *);
static vio_status_t setSsrState(vio_ssr_state_t);
static vio_status_t getSsrState(vio_ssr_state_t *curState);

static void sendData(const char *buf, uint8_t len);
static void USB_SEND(const char *string, ...);


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
	char respParam[48] = "";

	char *pLeftover;
	int32_t cmd = strtol(inBuf, &pLeftover, 10);
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
			result = setSsrState(strtol(pLeftover, NULL, 10));
			break;
		}

		case VIO_CMD_GET_SSR_STATE:
		{
			vio_ssr_state_t curState;
			result = getSsrState(&curState);
			if (result == VIO_STATUS_OK)
				snprintf(respParam, sizeof(respParam), " %d", curState);
			break;
		}

		case VIO_CMD_SEQ_ADD:
		{
			result = varPwmAdd(strtol(pLeftover, NULL, 10));
			break;
		}

		case VIO_CMD_SEQ_CLEAR:
		{
			result = varPwmClear();
			break;
		}

		default:
		{
			result = VIO_STATUS_BAD_COMMAND;
			break;
		}
	}

	char resp[64];
	snprintf(resp, sizeof(resp), "%s %d%s", inBuf, result, respParam);
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

static vio_status_t varPwmList(void)
{
	if (expReq.VarPwm.count <= 0)
		return VIO_STATUS_SEQUENCE_QUEUE_EMPTY;

	//should we check that all queued pulse widths are within the selected frequency?

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

static vio_status_t setSsrState(vio_ssr_state_t newState)
{
	if ((newState != VIO_SSR_OPEN) && (newState != VIO_SSR_CLOSED))
		return VIO_STATUS_BAD_PARAMETER;

	HAL_GPIO_WritePin(POWER_CONTROL_OUT_GPIO_Port, POWER_CONTROL_OUT_Pin, newState);
	return VIO_STATUS_OK;
}

static vio_status_t getSsrState(vio_ssr_state_t *curState)
{
	*curState = HAL_GPIO_ReadPin(POWER_CONTROL_OUT_GPIO_Port, POWER_CONTROL_OUT_Pin);
	return VIO_STATUS_OK;
}


static void sendData(const char *buf, uint8_t len)
{
	CDC_Transmit_FS(buf, len);
}

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
