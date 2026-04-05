/**
 * @file      main.c
 * @brief     main file
 * @author    white-xu
 * @date      2025-10-16
 * @version   1.0.0
 * @note      无
 */

#include "main.h"

/**
 * @brief main
 */
int main(void)
{
  /*Init*/
  init();

  /*task create*/
  task_init();

  /*启动任务调度*/
  vTaskStartScheduler();

  while (1)
    ; /*正常不会执行到这里*/
}
