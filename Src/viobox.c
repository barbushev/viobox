/*
 * viobox.c
 *	Varex IO Box
 *
 *  Created on: Mar 13, 2018
 *      Author: Ted Barbushev
 */
#include "viobox.h"
#include "vio_exp_req.h"
#include "vio_prep.h"

#include "usbd_cdc_if.h"
#include "usbd_desc.h"


/*
MAJOR.MINOR.PATCH, increment the:
MAJOR version when you make incompatible API changes,
MINOR version when you add functionality in a backwards-compatible manner, and
PATCH version when you make backwards-compatible bug fixes.*/

const uint8_t VIO_MAJOR_VERSION = 0;
const uint8_t VIO_MINOR_VERSION = 1;
const uint8_t VIO_PATCH_VERSION = 5;

const char VIO_COMM_DELIMETER = ' ';
const char VIO_COMM_TERMINATOR = '\n';

static vio_status_t vio_get_fw_ver(char *buf, uint8_t len);
static vio_status_t vio_get_ssr_state(char *buf, uint8_t len);
static vio_status_t vio_get_expok_count(char *buf, uint8_t len);
static vio_status_t vio_get_expok_notify(char *buf, uint8_t len);
static vio_status_t vio_get_serial_number(char *buf, uint8_t len);

static vio_status_t vio_set_ssr_state(const void *);
static vio_status_t vio_set_expok_count_zero();
static vio_status_t vio_set_expok_notify(const void *);
static vio_status_t vio_set_serial_number(const void *);

static vio_status_t vio_sys_reboot();
//static vio_status_t vio_sys_dfu();
static void vio_send_data(const char *buf, uint8_t len);

//this a jump table
static vio_status_t(*vio_get[])(char *, uint8_t) =
{
		[VIO_CMD_GET_FW_VER] = vio_get_fw_ver,
		[VIO_CMD_GET_SSR_STATE] = vio_get_ssr_state,
		[VIO_CMD_GET_EXPREQ_MIN_PERIOD_US] = vio_get_expreq_min_period_us,
		[VIO_CMD_GET_EXPREQ_MAX_PERIOD_US] = vio_get_expreq_max_period_us,
		[VIO_CMD_GET_EXPREQ_MAX_SEQUENCE_LENGTH] = vio_get_expreq_max_sequence_length,
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
		[VIO_CMD_GET_PREP_MIN_TIME_US] = vio_get_prep_min_time_us,
		[VIO_CMD_GET_PREP_MAX_TIME_US] = vio_get_prep_max_time_us,
		[VIO_CMD_GET_PREP_TIME_US] = vio_get_prep_timeus,
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

static vio_exposeok_t expOk =
{
		.count = 0,
		.notify = false,
};

void vio_init()
{
	vio_exp_req_init();
	vio_prep_init();
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

bool vio_is_valid_bool(const bool *value)
{
	return ((*value == true) || (*value == false));
}

static vio_status_t vio_get_fw_ver(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d.%d.%d", VIO_MAJOR_VERSION, VIO_MINOR_VERSION, VIO_PATCH_VERSION);
	return VIO_STATUS_OK;
}

static vio_status_t vio_get_ssr_state(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", HAL_GPIO_ReadPin(POWER_CONTROL_OUT_GPIO_Port, POWER_CONTROL_OUT_Pin));
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
