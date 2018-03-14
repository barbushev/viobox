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

static vio_status_t exposeReqSetState(vio_expreq_state_t);
static vio_status_t varPwmAdd(uint32_t);
static vio_status_t varPwmClear(void);
static vio_status_t varPwmList(void);

static vio_status_t isPeriodInRange(const uint32_t *);
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


vio_status_t vio_ProcessIncomingData(const char *inBuf, uint16_t len)
{
	vio_status_t result = 0;

	if (len == 0)
		return VIO_STATUS_BAD_COMMAND;

	char *pLeftover;
	int32_t cmd = strtol(inBuf, &pLeftover, 10);
	switch(cmd)
	{
		case VIO_CMD_GET_FW_VER:
		{
			USB_SEND("%d.%d.%d\n", VIO_MAJOR_VERSION, VIO_MINOR_VERSION, VIO_PATCH_VERSION);
			break;
		}

		case VIO_CMD_SET_EXPREQ_FREQUENCY:
		{
			strtol(pLeftover, NULL, 10);
			break;
		}

		case VIO_CMD_POWER_OFF:
		{

			break;
		}

		case VIO_CMD_POWER_ON:
		{

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

		case VIO_CMD_EXPREQ_START:
		{
			result = exposeReqSetState(VIO_EXPREQ_VARIABLE_PWM);
			break;
		}
	}

//sendResponse() send the original command + vio_status_t
	return result;
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

static vio_status_t isPeriodInRange(const uint32_t *fPeriodUs)
{
	if(((*fPeriodUs < VIO_MIN_PERIOD_US) && (*fPeriodUs > VIO_MAX_PERIOD_US)))
		return VIO_STATUS_OUT_OF_RANGE;

	return VIO_STATUS_OK;
}

static vio_status_t exposeReqSetState(vio_expreq_state_t newState)
{
	switch(newState)
	{
		case VIO_EXPREQ_IDLE:
		case VIO_EXPREQ_ONE_PULSE:
		case VIO_EXPREQ_FIXED_PWM:
		case VIO_EXPREQ_VARIABLE_PWM:
			break;
		default:
			return VIO_STATUS_GENERIC_ERROR;
	}

	return VIO_STATUS_OK;
}


static void prepareResponse(char *resp)
{

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
