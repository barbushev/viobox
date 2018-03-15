/*
 * viobox.h
 *
 *  Created on: Mar 13, 2018
 *      Author: Ted Barbushev
 */

#ifndef VIOBOX_H_
#define VIOBOX_H_

#include "stdint.h"

/*
MAJOR.MINOR.PATCH, increment the:
MAJOR version when you make incompatible API changes,
MINOR version when you add functionality in a backwards-compatible manner, and
PATCH version when you make backwards-compatible bug fixes.
*/

const uint8_t VIO_MAJOR_VERSION = 0;
const uint8_t VIO_MINOR_VERSION = 1;
const uint8_t VIO_PATCH_VERSION = 5;

const char VIO_COMM_DELIMETER[] = " ";

enum
{
	VIO_MAX_SEQUENCE_LENGTH = 1000,		/// Maximum number of elements in the variable pwm queue
	VIO_MIN_PERIOD_US = 10,				/// Minimum pulse length in microseconds
	VIO_MAX_PERIOD_US = 5000			/// Maximum pulse length in microseconds
};

typedef enum
{
	true,
	false
} bool;

typedef enum
{
	VIO_STATUS_OK,
	VIO_STATUS_GENERIC_ERROR,
	VIO_STATUS_SEQUENCE_IS_RUNNING,
	VIO_STATUS_SEQUENCE_QUEUE_FULL,
	VIO_STATUS_SEQUENCE_QUEUE_EMPTY,
	VIO_STATUS_OUT_OF_RANGE,
	VIO_STATUS_BAD_COMMAND,
	VIO_STATUS_BAD_PARAMETER,
}vio_status_t;

typedef enum
{
	VIO_CMD_REBOOT,				/// Reboot device
	VIO_CMD_DFU,				/// Reboot device in DFU (device firmware upgrade) mode
	VIO_CMD_GET_FW_VER,
	VIO_CMD_SET_EXPREQ_FREQUENCY,
	VIO_CMD_GET_EXPREQ_FREQUENCY,
	VIO_CMD_SET_EXPREQ_STATE,	/// Set the state of Expose Request. Requires a vio_expreq_state_t parameter.
	VIO_CMD_GET_EXPREQ_STATE,
	VIO_CMD_SET_SSR_STATE,		/// Set the state of Solid State Relay. Requires a vio_ssr_state_t parameter.
	VIO_CMD_GET_SSR_STATE,
	VIO_CMD_GET_MIN_PERIOD_US,
	VIO_CMD_GET_MAX_PERIOD_US,
	VIO_CMD_GET_MAX_SEQUENCE_LENGTH,
	VIO_CMD_SEQ_ADD,
	VIO_CMD_SEQ_CLEAR,
	cmdUserSyncEnable,
	cmdUserSyncDisable,
	cmdUserSyncFrequency,
	cmdUserSyncPulseWidth,
	cmdUserSyncOnePulse, 					//takes a uint32_t parameter with pulse length in ms. Generates one pulse with the specified pulse width and returns msgOnePulseComplete.
	cmdGetStatus,
	cmdQueueAdd,
	cmdQueueClear,
	cmdQueueList,
	cmdQueueStart,
	cmdQueueAbort,
	cmdQueueRunOnce,
	cmdQueueRunInfinite,
	cmdMcuReset,
	cmdSetOutputActiveHigh,
	cmdSetOutputActiveLow,
	cmdExtIntSyncEnable,
	cmdExtIntSyncDisable,
	cmdSetExposeSkip,				//takes a uint8_t parameter with number of pulses to skip.
	cmdUserSyncOnePulseStop,
}vio_commands_t;

typedef enum
{
	VIO_SSR_OPEN,
	VIO_SSR_CLOSED,
}vio_ssr_state_t;

typedef enum
{
	VIO_EXPREQ_IDLE,				   /// The device is idling
	VIO_EXPREQ_ONE_PULSE,	       /// The device is generating a single pulse.
	VIO_EXPREQ_FIXED_PWM,           /// The device is generating PWM with a fixed pulse width.
	VIO_EXPREQ_VARIABLE_PWM,        /// The device is generating PWM with variable pulse width using each sequence element.
}vio_expreq_state_t;

typedef struct
{
	uint32_t lengthUs;			/// Length of the single pulse in microseconds.
}vio_expreq_onepulse_t;

typedef struct
{
	uint32_t widthUs;		    /// Pulse width of fixed PWM in microseconds.
}vio_expreq_fixedpwm_t;

typedef struct
{
	uint16_t count;	 	  			/// Total number of elements in the sequence.
	uint16_t position;	      		/// Current position of the sequence.
	bool enableLooping;  		    /// Indicates if looping is enabled.
	volatile uint32_t numLoops;	    /// Number of completed loops.
	uint32_t elements[VIO_MAX_SEQUENCE_LENGTH];	/// Sequence pulse widths in microseconds.
	volatile bool waitForExtSync;   /// Sequence shall wait for external sync before starting.
}vio_expreq_varpwm_t;

typedef struct
{
	volatile vio_expreq_state_t State;  /// Indicates the current state of the device.
	vio_expreq_onepulse_t OnePulse;	    /// Properties of one pulse.
	vio_expreq_fixedpwm_t FixedPwm;     /// Properties of fixed PWM.
	vio_expreq_varpwm_t VarPwm;         /// Properties of variable PWM.
	uint32_t fPeriodUs;		            /// Period in microseconds. It is common between fixed and variable PWM.
}vio_expose_request_t;

typedef struct
{
	uint32_t timeUs;                    /// The length of time the prepare output is held high
	bool	 isRunning;					/// Prepare is currently producing a pulse
}vio_prepare_t;

typedef struct
{
	uint32_t count;						/// Number of Falling edges detected so far.
	bool notify;						/// When set to true, it sends a message every time a Falling edge is detected.
}vio_exposeok_t;

#endif /* VIOBOX_H_ */
