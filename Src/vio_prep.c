/*
 * vio_prep.c
 *
 *  Created on: Mar 19, 2018
 *      Author: Ted
 */
#include "viobox.h"
#include "vio_prep.h"
#include "stm32f1xx_hal.h"

#define PREPARE_OUT_Pin GPIO_PIN_15
#define PREPARE_OUT_GPIO_Port GPIOA

static const uint32_t VIO_PREP_MIN_PERIOD_US = 1000;			/// Minimum pulse length in microseconds. 1000Hz
static const uint32_t VIO_PREP_MAX_PERIOD_US = 100000000;		/// Maximum pulse length in microseconds. 0.01 Hz  (100 sec)

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

	  TIM_MasterConfigTypeDef sMasterConfig;
	  TIM_OC_InitTypeDef sConfigOC;

	  htim2.Instance = TIM2;
	  htim2.Init.Prescaler = 35999;
	  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	  htim2.Init.Period = 0;
	  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;  // 72MHz / 4 = 18
	  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)   //HAL_TIM_Base_Init
	  {
	    _Error_Handler(__FILE__, __LINE__);
	  }

	  if (HAL_TIM_OnePulse_Init(&htim2, TIM_OPMODE_SINGLE) != HAL_OK)
	  {
	    _Error_Handler(__FILE__, __LINE__);
	  }

	  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
	  {
	    _Error_Handler(__FILE__, __LINE__);
	  }

	  sConfigOC.OCMode = TIM_OCMODE_PWM1;
	  sConfigOC.Pulse = 0;
	  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
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
		prep.timeUs = *(uint32_t *)newValue;

	return result;
}

vio_status_t vio_set_prep_isrunning(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else if(prep.timeUs <= 0)
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

	return result;
}

vio_status_t vio_set_prep_notify(const void *newValue)
{
	vio_status_t result = VIO_STATUS_OK;
	if(!vio_is_valid_bool((bool *)newValue))
		result = VIO_STATUS_BAD_PARAMETER;
	else
		prep.notify = *(bool *)newValue;

	return result;
}
