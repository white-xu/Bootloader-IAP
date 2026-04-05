#include "main.h"

#include "mqttclient.h"
#include "cJSON_Process.h"
#include "./dwt_delay/core_delay.h"  
#include "bsp_dht11.h"

QueueHandle_t MQTT_Data_Queue =NULL;

DHT11_Data_TypeDef DHT11_Data;

extern void TCPIP_Init(void);


/*****************************************************************
  * @brief  主函数
  * @param  无
  * @retval 无
  * @note   第一步：开发板硬件初始化 
            第二步：创建APP应用任务
            第三步：启动FreeRTOS，开始多任务调度
  ****************************************************************/
int main(void)
{	  
  /* 开发板硬件初始化 */
  BSP_Init();

  /*task create*/
  task_init();

  /*启动任务调度*/
  vTaskStartScheduler();
  
  while(1);   /* 正常不会执行到这里 */    
}

