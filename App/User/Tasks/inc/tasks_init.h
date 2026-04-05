#ifndef __TASKS_INIT_H
#define __TASKS_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 任务数量
 */
#define TASK_COUNT 1

/*
 * 任务句柄
 */
/**
 * @brief 测试任务句柄
 */
extern TaskHandle_t test_task_handle;

/**
 * @brief  User task initialization
 * @param  None
 * @retval None
 */
void task_init(void);

#ifdef __cplusplus
}
#endif

#endif
