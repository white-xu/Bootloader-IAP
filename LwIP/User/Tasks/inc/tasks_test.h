#ifndef __TASKS_TEST_H
#define __TASKS_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

void test_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
