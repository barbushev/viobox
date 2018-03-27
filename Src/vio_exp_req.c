/*
 * exp_req_ctrl.c
 *
 *  Created on: Mar 18, 2018
 *      Author: Ted
 */
#include "viobox.h"
#include "vio_exp_req.h"
#include "string.h"
#include "stm32f1xx_hal.h"

#define __HAL_TIM_GET_PRESCALER(__HANDLE__)       ((__HANDLE__)->Instance->PSC)

#define EXPOSE_REQUEST_OUT_Pin GPIO_PIN_8
#define EXPOSE_REQUEST_OUT_GPIO_Port GPIOA
static const uint32_t VIO_EXPREQ_MIN_PERIOD_US = 1000;			/// Minimum period length in microseconds. 1000Hz
static const uint32_t VIO_EXPREQ_MAX_PERIOD_US = 10000000;		/// Maximum period length in microseconds. 0.1 Hz
static const uint32_t VIO_EXPREQ_MIN_PULSE_US = 1;				/// Minimum pulse width in microseconds.
static const uint32_t VIO_EXPREQ_MAX_PULSE_US = 10000001;		/// Maximum pulse width in microseconds. 0.1 Hz
static const uint32_t TIMER_RESOLUTION = 65536;                 /// The resulution of the PWM timer. We are using Timer1 and on STM32F103, it is 16bit.

extern const char VIO_COMM_DELIMETER;
extern const char VIO_COMM_TERMINATOR;


static TIM_HandleTypeDef htim1;
static bool process_send_varpwm_list = false;
static void vio_expreq_calc_timer_period(const uint32_t *pulseUs, uint32_t *returnPeriod);


static vio_expose_request_t expReq =
{
	.State = VIO_EXPREQ_IDLE,
	.fPeriodUs = 0,
	.OnePulse.lengthUs = 0,
	.FixedPwm.widthUs = 0,
	.FixedPwm.repeat = 0,
	.FixedPwm.private_counter = 0,
	.VarPwm.count = 0,
	.VarPwm.position = 0,
	.VarPwm.enableLooping = false,
	.VarPwm.numLoops = 0,
	.VarPwm.waitForExtSync = false,
	.notify = false,
};

vio_status_t vio_exp_req_init()
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

	TIM_ClockConfigTypeDef sClockSourceConfig;
	TIM_MasterConfigTypeDef sMasterConfig;
	TIM_OC_InitTypeDef sConfigOC;
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 1098;   ///default setting for 1 Hz
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 65514;	 ///default setting for 1 Hz
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; //needs to be enabled in order to change the frequency on the fly
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
    	_Error_Handler(__FILE__, __LINE__);
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
    {
    	_Error_Handler(__FILE__, __LINE__);
    }

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
    {
    	_Error_Handler(__FILE__, __LINE__);
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
    	_Error_Handler(__FILE__, __LINE__);
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;   //no pulse width
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
    	_Error_Handler(__FILE__, __LINE__);
    }

    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
    {
    	_Error_Handler(__FILE__, __LINE__);
    }

    GPIO_InitTypeDef GPIO_InitStruct;
    ///TIM1 GPIO Configuration     PA8     ------> TIM1_CH1
    GPIO_InitStruct.Pin = EXPOSE_REQUEST_OUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(EXPOSE_REQUEST_OUT_GPIO_Port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(TIM1_CC_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);

    return VIO_STATUS_OK;
}


/// Process tasks which can not be processed directly during USB interrupt
vio_status_t vio_expreq_process()
{
	vio_status_t result = VIO_STATUS_OK;

	if(process_send_varpwm_list == true)
	{
		uint32_t startTime;
		for(uint16_t i = 0; i < expReq.VarPwm.count; i++)
		{
			startTime = HAL_GetTick();
			do
			{
				result = vio_send_data_vararg("%d%c%u%c%lu%c", VIO_CMD_GET_EXPREQ_VARPWM_LIST, VIO_COMM_DELIMETER, i, VIO_COMM_DELIMETER, expReq.VarPwm.elements[i], VIO_COMM_TERMINATOR);
				if((HAL_GetTick() - startTime) > VIO_SYS_TIMEOUT_MS)
				{
					result = VIO_STATUS_SYS_TIMEOUT;
					goto error;
				}
			}
			while(result != VIO_STATUS_OK);
		}

		error:
		process_send_varpwm_list = false;
	}

	return result;
}

vio_status_t vio_get_expreq_min_period_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_EXPREQ_MIN_PERIOD_US);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_max_period_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_EXPREQ_MAX_PERIOD_US);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_min_pulse_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_EXPREQ_MIN_PULSE_US);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_max_pulse_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_EXPREQ_MAX_PULSE_US);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_max_sequence_length(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", VIO_EXPREQ_MAX_SEQUENCE_LENGTH);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_state(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.State);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_frequency(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.fPeriodUs);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_one_pulse_length(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.OnePulse.lengthUs);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_fixpwm_pwidth(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.FixedPwm.widthUs);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_fixpwm_repeat(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.FixedPwm.repeat);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_varpwm_count(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.count);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_varpwm_position(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.position);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_varpwm_looping(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.enableLooping);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_varpwm_numloops(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", expReq.VarPwm.numLoops);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_varpwm_waitforextsync(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.waitForExtSync);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_varpwm_list(char *buf, uint8_t len)
{
	if(expReq.VarPwm.count <= 0)
		return VIO_STATUS_EXPREQ_VARPWM_SEQUENCE_EMPTY;

	process_send_varpwm_list = true;
	return VIO_STATUS_OK;
}

vio_status_t vio_get_expreq_notify(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.notify);
	return VIO_STATUS_OK;
}


////////////////////////////////////////////////////////////////////////

vio_status_t vio_set_expreq_one_pulse_length(const void *newValue)
{
	vio_status_t result = vio_is_value_in_range((uint32_t *)newValue, &VIO_EXPREQ_MIN_PULSE_US, &VIO_EXPREQ_MAX_PULSE_US);
	if (result == VIO_STATUS_OK)
		expReq.OnePulse.lengthUs = *(uint32_t *)newValue;

	return result;
}

vio_status_t vio_set_expreq_state(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;  //VIO_STATUS_EXPREQ_CHANGE_STATE_FAIL
	//Only allow to switch state from idle to another state or from another state to idle
	if((expReq.State != VIO_EXPREQ_IDLE) && (*(vio_expreq_state_t *)newValue != VIO_EXPREQ_IDLE))
	{
		result = VIO_STATUS_EXPREQ_STATE_NOT_ALLOWED;
		goto error;
	}

	switch(*(vio_expreq_state_t *)newValue)
	{
		case VIO_EXPREQ_IDLE:
		{
			HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
			expReq.State = VIO_EXPREQ_IDLE;
			break;
		}

		case VIO_EXPREQ_ONE_PULSE:
		{
			uint32_t pulseLength = 0;  //load the correct pulse width
			vio_expreq_calc_timer_period(&expReq.OnePulse.lengthUs, &pulseLength);
			__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulseLength);

			HAL_TIM_OnePulse_Start_IT(&htim1, TIM_CHANNEL_1);
			expReq.State = VIO_EXPREQ_ONE_PULSE;

			break;
		}

		case VIO_EXPREQ_FIXED_PWM:
		{
			result = vio_set_expreq_frequency(&(expReq.fPeriodUs)); //Just in case set the frequency. Should already be correct.
			if(result != VIO_STATUS_OK)
				break;
			expReq.FixedPwm.private_counter = 0;  //clear the counter
			uint32_t pulseWidth = 0;  //load the correct pulse width
			vio_expreq_calc_timer_period(&expReq.FixedPwm.widthUs, &pulseWidth);
			__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulseWidth);

			HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
			expReq.State = VIO_EXPREQ_FIXED_PWM;

			break;
		}

		case VIO_EXPREQ_VARIABLE_PWM:
		{
			if(expReq.VarPwm.count <= 0)
				result = VIO_STATUS_EXPREQ_VARPWM_SEQUENCE_EMPTY;
			else
			{
				result = vio_set_expreq_frequency(&(expReq.fPeriodUs)); //Just in case set the frequency. Should already be correct.
				if(result != VIO_STATUS_OK)
					break;

				//@ TODO before starting, check that all the queued up pulse widths fit the currently selected frequency and take some action?
				for(uint16_t pNum = 0; pNum < expReq.VarPwm.count; pNum++)
				{
					if(expReq.VarPwm.elements[pNum] > (expReq.fPeriodUs + 1)) __NOP();
				}

				expReq.VarPwm.position = 0;
				expReq.VarPwm.numLoops = 0;

				uint32_t pulseWidth = 0;  //load the first pulse width
				vio_expreq_calc_timer_period(&expReq.VarPwm.elements[expReq.VarPwm.position], &pulseWidth);
				__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulseWidth);

				HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
				expReq.State = VIO_EXPREQ_VARIABLE_PWM;
			}
			break;
		}

		default:
			result = VIO_STATUS_BAD_PARAMETER;
	}

	error:
	return result;
}

vio_status_t vio_set_expreq_frequency(const void *newValue)
{
	vio_status_t result = vio_is_value_in_range((uint32_t *)newValue, &VIO_EXPREQ_MIN_PERIOD_US, &VIO_EXPREQ_MAX_PERIOD_US);
	if(result == VIO_STATUS_OK)
	{
		expReq.fPeriodUs = *(uint32_t *)newValue;

		uint32_t cycles = (HAL_RCC_GetPCLK2Freq() / (1000000.0 / *(uint32_t *)newValue));  //1000000 / microseconds - converting back to Hz
		uint32_t prescaler = (cycles / TIMER_RESOLUTION) - 1;
		if(cycles % TIMER_RESOLUTION != 0) prescaler++;  //if there was a left over, increment by 1
		uint32_t period = (float)cycles / (float)(prescaler + 1); //perform division as float and add 1 back to prescaler.

		__HAL_TIM_SET_PRESCALER(&htim1, prescaler);
		__HAL_TIM_SET_AUTORELOAD(&htim1, period);
	}

	return result;
}

vio_status_t vio_set_expreq_fixpwm_pwidth(const void *newValue)
{
	vio_status_t result = vio_is_value_in_range((uint32_t *)newValue, &VIO_EXPREQ_MIN_PULSE_US, &VIO_EXPREQ_MAX_PULSE_US);
	if (result == VIO_STATUS_OK)
	{
		expReq.FixedPwm.widthUs = *(uint32_t *)newValue;
		uint32_t newPeriod = 0;
		vio_expreq_calc_timer_period((uint32_t *)newValue, &newPeriod);

		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, newPeriod);
	}

	return result;
}

//Calculates the capture/compare register value to achieve a certain pulse width using the current prescaler setting of the timer.
static void vio_expreq_calc_timer_period(const uint32_t *pulseUs, uint32_t *returnPeriod)
{
	uint32_t cycles = (HAL_RCC_GetPCLK2Freq() / (1000000.0 / *pulseUs));  //1000000.0 / *pulseUs converts microseconds to Hz
	*returnPeriod = (float)cycles / (float)(__HAL_TIM_GET_PRESCALER(&htim1) + 1);

	//clamp the period to the max timer resolution. This will only be needed if a bad combination of frequency and pulse widths have been chosen. Example when the pulse width is wider the the frequency period.
	if(*returnPeriod > (TIMER_RESOLUTION - 1))   // @ TODO Should we return VIO_STATUS_OUT_OF_RANGE?
			*returnPeriod = TIMER_RESOLUTION - 1;
}

vio_status_t vio_set_expreq_fixpwm_repeat(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(expReq.State == VIO_EXPREQ_FIXED_PWM)
		result = VIO_STATUS_EXPREQ_FIXPWM_IS_RUNNING;
	else
		expReq.FixedPwm.repeat = *(uint32_t *)newValue;
	return result;
}

vio_status_t vio_set_expreq_varpwm_looping(const void *newValue)
{
	vio_status_t result = vio_is_valid_bool((bool *)newValue);
	if(result == VIO_STATUS_OK)
		expReq.VarPwm.enableLooping = *(bool *)newValue;

	return result;
}

vio_status_t vio_set_expreq_varpwm_waitforextsync(const void *newValue)
{
	vio_status_t result = vio_is_valid_bool((bool *)newValue);
	if(result == VIO_STATUS_OK)
		expReq.VarPwm.waitForExtSync = *(bool *)newValue;

	return result;
}

vio_status_t vio_set_expreq_varpwm_empty()
{
	vio_status_t result = VIO_STATUS_OK;
	if (expReq.State == VIO_EXPREQ_VARIABLE_PWM)
		result = VIO_STATUS_EXPREQ_VARPWM_IS_RUNNING;
	else
	{
		memset(expReq.VarPwm.elements, 0, VIO_EXPREQ_MAX_SEQUENCE_LENGTH);
		expReq.VarPwm.count = 0;
	}

	return result;
}

vio_status_t vio_set_expreq_varpwm_add(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if((expReq.VarPwm.count >= VIO_EXPREQ_MAX_SEQUENCE_LENGTH))
	{
		result = VIO_STATUS_EXPREQ_VARPWM_SEQUENCE_FULL;
		goto error;
	}

	if(expReq.State == VIO_EXPREQ_VARIABLE_PWM)
	{
		result = VIO_STATUS_EXPREQ_VARPWM_IS_RUNNING;
		goto error;
	}

	result = vio_is_value_in_range((uint32_t *)newValue, &VIO_EXPREQ_MIN_PULSE_US, &VIO_EXPREQ_MAX_PULSE_US);
	if(result == VIO_STATUS_OK)
		expReq.VarPwm.elements[expReq.VarPwm.count++] = *(uint32_t *)newValue;

	error:
	return result;
}

vio_status_t vio_set_expreq_notify(const void *newValue)
{
	vio_status_t result = vio_is_valid_bool((bool *)newValue);
	if(result == VIO_STATUS_OK)
		expReq.notify = *(bool *)newValue;

	return result;
}

void TIM1_CC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	switch(expReq.State)
	{
	case VIO_EXPREQ_ONE_PULSE:
		{
			HAL_TIM_OnePulse_Stop_IT(&htim1, TIM_CHANNEL_1);
			expReq.State = VIO_EXPREQ_IDLE;
			if(expReq.notify == true)
				vio_send_data_vararg("One Pulse%c", VIO_COMM_TERMINATOR);

			break;
		}
	case VIO_EXPREQ_FIXED_PWM:
		{
			expReq.FixedPwm.private_counter++;
			//if repeat is 0, it runs continuous otherwise it stops as soon as private_counter reaches repeat's value
			if((expReq.FixedPwm.repeat != 0) && (expReq.FixedPwm.private_counter >= expReq.FixedPwm.repeat))
			{
				HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1);
				expReq.State = VIO_EXPREQ_IDLE;
				if(expReq.notify == true)
					vio_send_data_vararg("%d%c%lu%c", VIO_STATUS_EXPREQ_COMPLETED, VIO_COMM_DELIMETER, expReq.FixedPwm.private_counter, VIO_COMM_TERMINATOR);
			}
			break;
		}

	case VIO_EXPREQ_VARIABLE_PWM:
		{
			expReq.VarPwm.position++;
			if(expReq.VarPwm.position >= expReq.VarPwm.count)
			{
				expReq.VarPwm.numLoops++;
				if(expReq.VarPwm.enableLooping == true)
				{
					expReq.VarPwm.position = 0;
					if(expReq.notify == true)
						vio_send_data_vararg("%d%c%lu%c", VIO_STATUS_EXPREQ_LOOP_NUM, VIO_COMM_DELIMETER, expReq.VarPwm.numLoops, VIO_COMM_TERMINATOR);
				}
				else
				{
					HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1);
					expReq.State = VIO_EXPREQ_IDLE;
					if(expReq.notify == true)
						vio_send_data_vararg("%d%c", VIO_STATUS_EXPREQ_COMPLETED, VIO_COMM_TERMINATOR);
					break;
				}
			}

			uint32_t nextPeriod = 0;
			vio_expreq_calc_timer_period(&(expReq.VarPwm.elements[expReq.VarPwm.position]), &nextPeriod);
			__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, nextPeriod);
			break;
		}

	case VIO_EXPREQ_IDLE:	//if state was idle or not any of the other defined states, there is a problem.
	default:
		{
			HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1);
			vio_send_data_vararg("%d%c", VIO_STATUS_SYS_ERROR, VIO_COMM_TERMINATOR);

			break;
		}
	}
}

void vio_expreq_irq_callback()
{

}
