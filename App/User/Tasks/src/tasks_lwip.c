#define LOG_TAG "tasks_lwip"
#define LOG_LVL ELOG_LVL_VERBOSE
#include "elog.h"

#include "tasks_lwip.h"
#include "sys_arch.h"

void lwip_bootstrap_task(void *pvParameters)
{
    (void)pvParameters;

    log_i("start lwip bootstrap");
    TCPIP_Init();
    log_i("lwip bootstrap complete");

    vTaskDelete(NULL);
}
