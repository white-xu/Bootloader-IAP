#define LOG_TAG "tasks_test"
#define LOG_LVL ELOG_LVL_VERBOSE
#include "elog.h"

#include "tasks_test.h"

void test_task(void *pvParameters)
{
    log_i("test task run");
    while(1)
    {
        log_i("test task");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
