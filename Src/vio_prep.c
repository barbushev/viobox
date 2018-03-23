/*
 * vio_prep.c
 *
 *  Created on: Mar 19, 2018
 *      Author: Ted
 */
#include "viobox.h"
#include "vio_prep.h"
#include "string.h"
#include "stm32f1xx_hal.h"

#define PREPARE_OUT_Pin GPIO_PIN_15
#define PREPARE_OUT_GPIO_Port GPIOA

static const uint32_t VIO_PREP_MIN_PERIOD_US = 1000;			/// Minimum pulse length in microseconds. 1000Hz
static const uint32_t VIO_PREP_MAX_PERIOD_US = 100000000;		/// Maximum pulse length in microseconds. 0.01 Hz  (100 sec)
extern const char VIO_COMM_TERMINATOR;

static TIM_HandleTypeDef htim2;
static vio_prepare_t prep =
{
		.timeUs = 0,
		.isRunning = false,
		.notify = false,
};

vio_status_t vio_prep_init()
{
	//TIM2 is clocked from APB1. APB1 timer clock is configured to run at 72 MHz
	//Since prep requires longer periods, we divide the clock by 4. 72 / 4 = 18.
	TIM_OnePulse_InitTypeDef sConfigOC;

	  htim2.Instance = TIM2;
	  htim2.Init.Prescaler = 35999;
	  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	  htim2.Init.Period = 0;
	  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;  // 72MHz / 4 = 18
	  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	  if (HAL_TIM_OnePulse_Init(&htim2, TIM_OPMODE_SINGLE) != HAL_OK)
	  {
		  _Error_Handler(__FILE__, __LINE__);
	  }

	  sConfigOC.OCMode = TIM_OCMODE_ACTIVE;
	  sConfigOC.Pulse = 0;   //no pulse width
	  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	  if(HAL_TIM_OnePulse_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1, TIM_CHANNEL_2) != HAL_OK)
	  {
		  _Error_Handler(__FILE__, __LINE__);
	  }

	  ///TIM2 GPIO Configuration   PA15     ------> TIM2_CH1
	  GPIO_InitTypeDef GPIO_InitStruct;
      GPIO_InitStruct.Pin = PREPARE_OUT_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	  HAL_GPIO_Init(PREPARE_OUT_GPIO_Port, &GPIO_InitStruct);

	  __HAL_AFIO_REMAP_TIM2_PARTIAL_1();
	  return VIO_STATUS_OK;
}

vio_status_t vio_get_prep_min_time_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_PREP_MIN_PERIOD_US);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_prep_max_time_us(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", VIO_PREP_MAX_PERIOD_US);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_prep_timeus(char *buf, uint8_t len)
{
	snprintf(buf, len, "%lu", prep.timeUs);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_prep_isrunning(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", prep.isRunning);
	return VIO_STATUS_OK;
}

vio_status_t vio_get_prep_notify(char *buf, uint8_t len)
{
	snprintf(buf, len, "%d", prep.notify);
	return VIO_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////


vio_status_t vio_set_prep_timeus(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(prep.isRunning == true)
		result = VIO_STATUS_PREP_ALREADY_RUNNING;
	else
		result = vio_is_value_in_range((uint32_t *)newValue, &VIO_PREP_MIN_PERIOD_US, &VIO_PREP_MAX_PERIOD_US);

	if(result == VIO_STATUS_OK)
		prep.timeUs = *(uint32_t *)newValue;

	return result;
}

vio_status_t vio_set_prep_isrunning(const void *newValue)
{
	vio_status_t result = vio_is_valid_bool((bool *)newValue);
	if(result == VIO_STATUS_OK)
	{
		if(prep.timeUs <= 0)
			result = VIO_STATUS_PREP_NOT_CONFIGURED;
		else if((prep.isRunning == true) && *(bool *)newValue == true)
			result = VIO_STATUS_PREP_ALREADY_RUNNING;
		else
		{
			if(*(bool *)newValue == true)
				HAL_TIM_OnePulse_Start_IT(&htim2, TIM_CHANNEL_1);
			else
				HAL_TIM_OnePulse_Stop_IT(&htim2, TIM_CHANNEL_1);

			prep.isRunning = *(bool *)newValue;
		}
	}
	return result;
}

vio_status_t vio_set_prep_notify(const void *newValue)
{
	vio_status_t result = vio_is_valid_bool((bool *)newValue);
	if(result == VIO_STATUS_OK)
		prep.notify = *(bool *)newValue;

	return result;
}

void vio_prep_irq_callback()
{

}

void TIM2_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&htim2);
	prep.isRunning = false;

	if(prep.notify == true)
		vio_send_data_vararg("%d%c", VIO_STATUS_PREP_COMPLETED, VIO_COMM_TERMINATOR);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

}
