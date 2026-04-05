#define LOG_TAG "tasks_test"
#define LOG_LVL ELOG_LVL_VERBOSE
#include "elog.h"

#include "stm32f4xx.h"
#include "tasks_test.h"

#define TEST_TASK_FAULT_INJECT_ENABLE      1
#define TEST_TASK_FAULT_INJECT_DELAY_MS    3000U
#define TEST_TASK_FAULT_TYPE_DIV_ZERO      1
#define TEST_TASK_FAULT_TYPE_BAD_JUMP      2
#define TEST_TASK_FAULT_INJECT_TYPE        TEST_TASK_FAULT_TYPE_DIV_ZERO

static void test_task_trigger_fault(void)
{
#if TEST_TASK_FAULT_INJECT_TYPE == TEST_TASK_FAULT_TYPE_DIV_ZERO
    volatile int numerator = 1;
    volatile int denominator = 0;
    volatile int result;

    /* Enable divide-by-zero trapping so the fault is routed into CmBacktrace. */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    __DSB();
    __ISB();

    result = numerator / denominator;
    (void)result;
#elif TEST_TASK_FAULT_INJECT_TYPE == TEST_TASK_FAULT_TYPE_BAD_JUMP
    typedef void (*fault_entry_t)(void);
    fault_entry_t invalid_entry = (fault_entry_t)0xFFFFFFFFU;

    invalid_entry();
#else
    #error "Unsupported TEST_TASK_FAULT_INJECT_TYPE"
#endif
}

void test_task(void *pvParameters)
{
    (void)pvParameters;

    log_i("test task run");

#if TEST_TASK_FAULT_INJECT_ENABLE
    #if TEST_TASK_FAULT_INJECT_TYPE == TEST_TASK_FAULT_TYPE_DIV_ZERO
    log_w("fault injection enabled, divide-by-zero will trigger in %lu ms", (unsigned long)TEST_TASK_FAULT_INJECT_DELAY_MS);
    #elif TEST_TASK_FAULT_INJECT_TYPE == TEST_TASK_FAULT_TYPE_BAD_JUMP
    log_w("fault injection enabled, invalid jump will trigger in %lu ms", (unsigned long)TEST_TASK_FAULT_INJECT_DELAY_MS);
    #endif
    vTaskDelay(pdMS_TO_TICKS(TEST_TASK_FAULT_INJECT_DELAY_MS));
    #if TEST_TASK_FAULT_INJECT_TYPE == TEST_TASK_FAULT_TYPE_DIV_ZERO
    log_w("trigger divide-by-zero fault for CmBacktrace test");
    #elif TEST_TASK_FAULT_INJECT_TYPE == TEST_TASK_FAULT_TYPE_BAD_JUMP
    log_w("trigger invalid jump fault for CmBacktrace test");
    #endif
    test_task_trigger_fault();
#endif

    while(1)
    {
        log_i("test task");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
