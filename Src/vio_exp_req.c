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
static const uint32_t VIO_EXPREQ_MIN_PERIOD_US = 1000;			/// Minimum pulse length in microseconds. 1000Hz
static const uint32_t VIO_EXPREQ_MAX_PERIOD_US = 10000000;		/// Maximum pulse length in microseconds. 0.1 Hz

extern const char VIO_COMM_DELIMETER;
extern const char VIO_COMM_TERMINATOR;


static TIM_HandleTypeDef htim1;

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

static vio_status_t vio_expreq_is_period_in_range(const uint32_t *fPeriodUs);

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

  return VIO_STATUS_OK;
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

vio_status_t vio_get_expreq_varpwm_notify(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", expReq.VarPwm.notify);
	return VIO_STATUS_OK;
}


////////////////////////////////////////////////////////////////////////

vio_status_t vio_set_expreq_one_pulse_length(const void *newValue)
{
	vio_status_t result = vio_expreq_is_period_in_range((uint32_t *)newValue);
	if (result == VIO_STATUS_OK)
		expReq.OnePulse.lengthUs = *(uint32_t *)newValue;

	return result;
}

vio_status_t vio_set_expreq_state(const void *newValue)
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

vio_status_t vio_set_expreq_frequency(const void *newValue)
{
	vio_status_t result = vio_expreq_is_period_in_range((uint32_t *)newValue);
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

//		char buf[64];   //the stuff below is used for debugging. Will remove later.
//		snprintf(buf, sizeof(buf), "cyl: %lu, scl: %lu, per: %lu\n", cycles, prescaler, period);
//		CDC_Transmit_FS(buf, strlen(buf));
	}

	return result;
}

vio_status_t vio_set_expreq_fixpwm_pwidth(const void *newValue)
{
	vio_status_t result = vio_expreq_is_period_in_range((uint32_t *)newValue);
	if (result == VIO_STATUS_OK)
	{
		uint32_t cycles = (HAL_RCC_GetPCLK2Freq() / (1000000.0 / *(uint32_t *)newValue));
		uint32_t period = (float)cycles / (float)(__HAL_TIM_GET_PRESCALER(&htim1) + 1);

		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, period);
		expReq.FixedPwm.widthUs = *(uint32_t *)newValue;
	}

	return result;
}

vio_status_t vio_set_expreq_varpwm_looping(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		expReq.VarPwm.enableLooping = *(bool *)newValue;

	return result;
}

vio_status_t vio_set_expreq_varpwm_waitforextsync(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
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

	result = vio_expreq_is_period_in_range((uint32_t *)newValue);
	if(result == VIO_STATUS_OK)
		expReq.VarPwm.elements[expReq.VarPwm.count++] = *(uint16_t *)newValue;

	error:
	return result;
}

vio_status_t vio_set_expreq_varpwm_notify(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		expReq.VarPwm.notify = *(bool *)newValue;

	return result;
}

static vio_status_t vio_expreq_is_period_in_range(const uint32_t *fPeriodUs)
{
	if(((*fPeriodUs < VIO_EXPREQ_MIN_PERIOD_US) || (*fPeriodUs > VIO_EXPREQ_MAX_PERIOD_US)))
		return VIO_STATUS_OUT_OF_RANGE;

	return VIO_STATUS_OK;
}

void TIM1_CC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
}

void vio_expreq_irq_callback()
{
	///on completion of each loop
	if((expReq.VarPwm.notify == true) && (expReq.State == VIO_EXPREQ_VARIABLE_PWM))
	{
		char msg[16];
		if(expReq.VarPwm.enableLooping == false)
			snprintf(msg, sizeof(msg), "%d%c", VIO_STATUS_EXPREQ_COMPLETED, VIO_COMM_TERMINATOR);
		else
			snprintf(msg, sizeof(msg), "%d%c%lu%c", VIO_STATUS_EXPREQ_LOOP_NUM, VIO_COMM_DELIMETER, expReq.VarPwm.numLoops, VIO_COMM_TERMINATOR);

		vio_send_data(msg, strlen(msg));
	}

	if((expReq.State == VIO_EXPREQ_ONE_PULSE) || (expReq.State == VIO_EXPREQ_FIXED_PWM))
		expReq.State = VIO_EXPREQ_IDLE;

	if((expReq.State == VIO_EXPREQ_VARIABLE_PWM) && (expReq.VarPwm.enableLooping == false))
		expReq.State = VIO_EXPREQ_IDLE;
}
