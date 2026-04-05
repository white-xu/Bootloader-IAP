#include "tasks_test.h"
#include "./debug/debug.h"

void test_task(void *pvParameters)
{
    PRINT_INFO("test task run");
    while(1)
    {
        PRINT_INFO("test task");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
