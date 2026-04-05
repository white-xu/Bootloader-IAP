#ifndef __TASKS_LWIP_H
#define __TASKS_LWIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"

void lwip_bootstrap_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
