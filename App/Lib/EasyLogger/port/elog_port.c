/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */

#include <elog.h>
#include <stdio.h>

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define ELOG_UART_BAUDRATE        115200U
#define ELOG_UART_INSTANCE        USART1
#define ELOG_UART_TX_GPIO_PORT    GPIOA
#define ELOG_UART_TX_PIN          GPIO_PIN_9
#define ELOG_UART_TX_AF           GPIO_AF7_USART1
#define ELOG_UART_RX_GPIO_PORT    GPIOA
#define ELOG_UART_RX_PIN          GPIO_PIN_10
#define ELOG_UART_RX_AF           GPIO_AF7_USART1
#define ELOG_UART_TIMEOUT_MS      1000U

static UART_HandleTypeDef s_elog_uart;
static SemaphoreHandle_t s_elog_lock = NULL;

static int elog_port_is_unlocked_context(void) {
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U)) {
        return 1;
    }

    return xTaskGetSchedulerState() != taskSCHEDULER_RUNNING;
}

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;

    s_elog_uart.Instance = ELOG_UART_INSTANCE;
    s_elog_uart.Init.BaudRate = ELOG_UART_BAUDRATE;
    s_elog_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_elog_uart.Init.StopBits = UART_STOPBITS_1;
    s_elog_uart.Init.Parity = UART_PARITY_NONE;
    s_elog_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_elog_uart.Init.Mode = UART_MODE_TX_RX;
    s_elog_uart.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&s_elog_uart);

    if (s_elog_lock == NULL) {
        s_elog_lock = xSemaphoreCreateMutex();
    }
    
    return result;
}

/**
 * EasyLogger port deinitialize
 *
 */
void elog_port_deinit(void) {
    if (s_elog_lock != NULL) {
        vSemaphoreDelete(s_elog_lock);
        s_elog_lock = NULL;
    }

    HAL_UART_DeInit(&s_elog_uart);

}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
    while (size > 0U) {
        uint16_t chunk_size = size > UINT16_MAX ? UINT16_MAX : (uint16_t) size;
        HAL_UART_Transmit(&s_elog_uart, (uint8_t *) log, chunk_size, ELOG_UART_TIMEOUT_MS);
        log += chunk_size;
        size -= chunk_size;
    }
}

/**
 * output lock
 */
void elog_port_output_lock(void) {
    if ((s_elog_lock != NULL) && !elog_port_is_unlocked_context()) {
        xSemaphoreTake(s_elog_lock, portMAX_DELAY);
    }
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
    if ((s_elog_lock != NULL) && !elog_port_is_unlocked_context()) {
        xSemaphoreGive(s_elog_lock);
    }
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void) {
    static char cur_system_time[16];
    uint32_t tick = HAL_GetTick();

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        tick = xTaskGetTickCount();
    }

    snprintf(cur_system_time, sizeof(cur_system_time), "%lu", tick);
    return cur_system_time;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    return "";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    if (__get_IPSR() != 0U) {
        return "isr";
    }

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return "main";
    }

    return pcTaskGetName(NULL);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance != ELOG_UART_INSTANCE) {
        return;
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = ELOG_UART_TX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = ELOG_UART_TX_AF;
    HAL_GPIO_Init(ELOG_UART_TX_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ELOG_UART_RX_PIN;
    GPIO_InitStruct.Alternate = ELOG_UART_RX_AF;
    HAL_GPIO_Init(ELOG_UART_RX_GPIO_PORT, &GPIO_InitStruct);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart) {
    if (huart->Instance != ELOG_UART_INSTANCE) {
        return;
    }

    HAL_GPIO_DeInit(ELOG_UART_TX_GPIO_PORT, ELOG_UART_TX_PIN);
    HAL_GPIO_DeInit(ELOG_UART_RX_GPIO_PORT, ELOG_UART_RX_PIN);
    __HAL_RCC_USART1_CLK_DISABLE();
}
