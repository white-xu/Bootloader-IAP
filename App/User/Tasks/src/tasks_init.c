#define LOG_TAG "tasks_init"
#define LOG_LVL ELOG_LVL_VERBOSE
#include "elog.h"

#include "tasks_init.h"

#include "tasks_lwip.h"
#include "tasks_test.h"

/*
 * 任务创建管理
 */
/**
 * @brief 创建任务参数结构体
 */
typedef struct
{
    TaskFunction_t pvTaskCode;   // 任务函数指针
    const char *pcName;          // 任务名称
    uint16_t usStackDepth;       // 栈深度（以字为单位）
    void *pvParameters;          // 任务参数
    UBaseType_t uxPriority;      // 任务优先级
    TaskHandle_t *pvCreatedTask; // 任务句柄指针
} TaskCreateParams_t;

/**
 * @brief 测试任务句柄
 */
TaskHandle_t test_task_handle = NULL;

/**
 * @brief lwIP 启动任务句柄
 */
TaskHandle_t lwip_bootstrap_task_handle = NULL;

/**
 * @brief 升级任务句柄
 */
TaskHandle_t upgrade_task_handle = NULL;

/**
 * @brief GUI任务句柄
 */
TaskHandle_t gui_task_handle = NULL;

/**
 * @brief 任务配置结构体
 */
static TaskCreateParams_t taskParamsArray[TASK_COUNT] = {
   {
       .pvTaskCode = lwip_bootstrap_task,
       .pcName = "lwip_bootstrap",
       .usStackDepth = 1024,
       .pvParameters = NULL,
       .uxPriority = 3,
       .pvCreatedTask = &lwip_bootstrap_task_handle
   },
   {
       .pvTaskCode = test_task,
       .pcName = "test_task",
       .usStackDepth = 512,
       .pvParameters = NULL,
       .uxPriority = 2,
       .pvCreatedTask = &test_task_handle
   }
};

/**
  * @brief  批量创建任务
  * @param  paramsArray: 任务参数结构体数组指针，包含所有待创建任务的参数
  * @param  taskNum: 要创建的任务数量
  * @retval BaseType_t: 返回任务创建结果
  *         - pdPASS: 所有任务都创建成功
  *         - pdFAIL: 某个任务创建失败，后续任务不再创建
  * @note   none
  */
static BaseType_t createTasksFromArray(const TaskCreateParams_t *paramsArray, uint16_t taskNum)
{
    BaseType_t xResult = pdPASS;

    if(taskNum > TASK_COUNT)
        taskNum = TASK_COUNT;

    taskENTER_CRITICAL(); // 进入临界区
    for (uint32_t i = 0; i < taskNum; i++)
    {
        const TaskCreateParams_t *params = &paramsArray[i];
        xResult = xTaskCreate(
            params->pvTaskCode,
            params->pcName,
            params->usStackDepth,
            params->pvParameters,
            params->uxPriority,
            params->pvCreatedTask);

        if (xResult != pdPASS)
        {
            log_e("create task failed: %s", params->pcName);
            break;
        }
        else
        {
            log_i("create task success: %s", params->pcName);
        }
    }
    taskEXIT_CRITICAL();               // 退出临界区

    return xResult;
}

/*-----------------------------------------------------------*/

/**
 * @brief  User task initialization
 * @param  None
 * @retval None
 */
void task_init(void)
{
    createTasksFromArray(taskParamsArray, TASK_COUNT);
}
