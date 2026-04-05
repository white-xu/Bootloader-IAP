#include "tasks_test.h"
#include "debug.h"

#include <stdio.h>

// extern void TCPIP_Init(void);
// extern void mqtt_thread_init(void);

static void get_system_state(void)
{
    /* 获取栈使用情况 */
    size_t size1 = 0, size2 = 0;
    size1 = xPortGetFreeHeapSize();
    size2 = xPortGetMinimumEverFreeHeapSize();
    PRINT_INFO("Free Heap Size: %d", size1);
    PRINT_INFO("Minimum Ever Free Heap Size: %d", size2);

    /* 获取任务运行情况 */
    char * InfoBuffer= (char *)pvPortMalloc(200 * sizeof(char));
    if (InfoBuffer != NULL) 
    {
 		vTaskList(InfoBuffer);							            //获取所有任务的信息
        PRINT_INFO("任务名称\t 状态\t 优先级\t 剩余栈\t 编号");
        PRINT_INFO("B 阻塞态\tR 就绪态\tS 挂起态\tD 删除态\tR 运行态");
  		printf("%s\r\n",InfoBuffer);					//通过串口打印所有任务的信息
        vPortFree(InfoBuffer);  // 使用后释放
    }
}

void test_task(void *pvParameters)
{
    // PRINT_INFO("初始化LwIP协议");
    // TCPIP_Init();

    // PRINT_INFO("MQTT初始化");
    // mqtt_thread_init();

    uint8_t repead_test_count = 0;
    get_system_state();

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
        PRINT_INFO("测试任务运行");

        repead_test_count++;
        if(repead_test_count >= 5)
        {
            repead_test_count = 0;
            // get_system_state();
        }
    }
}
