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
PATCH version when you make backwards-compatible bug fixes.*/

const uint8_t VIO_MAJOR_VERSION = 0;
const uint8_t VIO_MINOR_VERSION = 1;
const uint8_t VIO_PATCH_VERSION = 5;
const uint32_t VIO_MIN_PERIOD_US = 10;				/// Minimum pulse length in microseconds
const uint32_t VIO_MAX_PERIOD_US = 5000;			/// Maximum pulse length in microseconds
enum{VIO_MAX_VARPWM_SEQUENCE_LENGTH = 5000};		/// Maximum number of elements in the variable pwm queue
const char VIO_COMM_DELIMETER = ' ';
const char VIO_COMM_TERMINATOR = '\n';

typedef enum
{
	false,
	true
} bool;

typedef enum
{
	VIO_STATUS_OK,
	VIO_STATUS_EXPOSEOK_EVENT,
	VIO_STATUS_GENERIC_ERROR,
	VIO_STATUS_VARPWM_IS_RUNNING,
	VIO_STATUS_SEQUENCE_QUEUE_FULL,
	VIO_STATUS_SEQUENCE_QUEUE_EMPTY,
	VIO_STATUS_PREP_IS_RUNNING,
	VIO_STATUS_PREP_NOT_CONFIGURED,
	VIO_STATUS_OUT_OF_RANGE,
	VIO_STATUS_BAD_COMMAND,
	VIO_STATUS_BAD_PARAMETER,
}vio_status_t;

typedef enum
{
	// GET commands
	VIO_CMD_GET_BLOCK_START,				   /// Marks the start of GET block. Returns VIO_STATUS_BAD_COMMAND.
	VIO_CMD_GET_FW_VER,
	VIO_CMD_GET_MIN_PERIOD_US,
	VIO_CMD_GET_MAX_PERIOD_US,
	VIO_CMD_GET_MAX_SEQUENCE_LENGTH,
	VIO_CMD_GET_SSR_STATE,
	VIO_CMD_GET_EXPREQ_STATE,
	VIO_CMD_GET_EXPREQ_FREQUENCY,
	VIO_CMD_GET_EXPREQ_ONE_PULSE_LENGTH,
	VIO_CMD_GET_EXPREQ_FIXPWM_PWIDTH,           ///
	VIO_CMD_GET_EXPREQ_VARPWM_COUNT,    		/// Returns the value of vio_expreq_varpwm_t.count
	VIO_CMD_GET_EXPREQ_VARPWM_POSITION, 		/// Returns the value of vio_expreq_varpwm_t.position
	VIO_CMD_GET_EXPREQ_VARPWM_LOOPING,  		/// Returns the value of vio_expreq_varpwm_t.enableLooping
	VIO_CMD_GET_EXPREQ_VARPWM_NUMLOOPS, 		/// Returns the value of vio_expreq_varpwm_t.numLoops
	VIO_CMD_GET_EXPREQ_VARPWM_WAITFOREXTSYNC,   /// Returns the value of vio_expreq_varpwm_t.waitForExtSync
	VIO_CMD_GET_EXPREQ_VARPWM_ELEMENT,          /// Takes a parameter of Element Number. Returns the value of that element if it exists.
	VIO_CMD_GET_PREP_TIMEUS,
	VIO_CMD_GET_PREP_ISRUNNING,
	VIO_CMD_GET_EXPOK_COUNT,
	VIO_CMD_GET_EXPOK_NOTIFY,
	VIO_CMD_GET_SERIAL_NUMBER,
	VIO_CMD_GET_BLOCK_END,						/// Marks the end of GET block. Returns VIO_STATUS_BAD_COMMAND.

	// System commands are located between GET_BLOCK_END and SET_BLOCK_START
	VIO_CMD_SYS_REBOOT,				/// Reboot device
	VIO_CMD_SYS_DFU,				/// Reboot device in DFU (device firmware upgrade) mode

	// SET commands
	VIO_CMD_SET_BLOCK_START,				   	/// Marks the start of SET block. Returns VIO_STATUS_BAD_COMMAND.
	VIO_CMD_SET_SSR_STATE,					   	/// Set the state of Solid State Relay. Requires a vio_ssr_state_t parameter.
	VIO_CMD_SET_EXPREQ_ONE_PULSE_LENGTH,		/// Set the length of One pulse. Requires a vio_expreq_onepulse_t.lengthUs parameter.
	VIO_CMD_SET_EXPREQ_STATE,				  	/// Set the state of Expose Request. Requires a vio_expreq_state_t parameter.
	VIO_CMD_SET_EXPREQ_FREQUENCY,				///
	VIO_CMD_SET_EXPREQ_FIXPWM_PWIDTH,			///
	VIO_CMD_SET_EXPREQ_VARPWM_LOOPING,  		/// Takes a parameter of bool (0 or 1). Enables or disables looping.
	VIO_CMD_SET_EXPREQ_VARPWM_WAITFOREXTSYNC,
	VIO_CMD_SET_EXPREQ_VARPWM_EMPTY,           /// Clear all vio_expreq_varpwm_t.elements
	VIO_CMD_SET_EXPREQ_VARPWM_ADD,			   /// Takes a parameter pulseWidthUs. Adds it to the end of the queue.
	VIO_CMD_SET_PREP_TIMEUS,
	VIO_CMD_SET_PREP_ISRUNNING,
	VIO_CMD_SET_EXPOK_COUNT_ZERO,
	VIO_CMD_SET_EXPOK_NOTIFY,
	VIO_CMD_SET_SERIAL_NUMBER,
	VIO_CMD_SET_BLOCK_END,				   	    /// Marks the end of SET block. Returns VIO_STATUS_BAD_COMMAND.
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
	uint16_t elements[VIO_MAX_VARPWM_SEQUENCE_LENGTH];	/// Sequence pulse widths in microseconds.
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
	volatile uint32_t count;			/// Number of Falling edges detected so far.
	bool notify;						/// When set to true, it sends a message every time a Falling edge is detected.
}vio_exposeok_t;

#endif /* VIOBOX_H_ */
