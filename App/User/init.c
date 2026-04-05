/*
 * include
 */
#include "stm32f4xx_hal.h"

/* SYSTEM */
#include "./debug/debug.h"
#include "./delay/delay.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FatFS */
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
/*-----------------------------------------------------------*/

/*
 * 外部变量声明
 */
/**
 * @brief
 */
extern Diskio_drvTypeDef SD_Driver;
/*-----------------------------------------------------------*/

/*
 * 全局变量声明
 */
/**
 * @brief pointer to the logical drive path
 */
char SDPath[4];
/*-----------------------------------------------------------*/

/*
 * 静态函数声明
 */
/**
 * @brief Configures system clock using HSE and PLL for 168MHz operation
 */
static void SystemClock_Config(void);

/**
 * @brief FatFS init
 */
static void fatfs_init(void);
/*-----------------------------------------------------------*/

/**
 * @brief init
 */
void init(void)
{
    HAL_Init();

    SystemClock_Config();

    /* SYSTEM Init*/
    CPU_TS_TmrInit();
    DEBUG_USART_Config();

    /* FatFS Init*/
    fatfs_init();
}

/*
 * 静态函数定义
 */
static FATFS fs; /* FatFs文件系统对象 */
/**
 * @brief FatFS init
 */
static void fatfs_init(void)
{
    FRESULT res_sd;

    FATFS_LinkDriver(&SD_Driver, SDPath); /* 链接驱动器，创建盘符 */
    res_sd = f_mount(&fs, SDPath, 1);       /* 在外部SD卡挂载文件系统，文件系统挂载时会对SD卡初始化 */
    if (res_sd == FR_NO_FILESYSTEM)       /* 如果没有文件系统就格式化创建创建文件系统 */
    {
        PRINT_INFO("Format the SD card");
        /* 格式化 */
        res_sd = f_mkfs(SDPath, 0, 0);

        if (res_sd == FR_OK)
        {
            PRINT_INFO("Formatting successful");
            /* 格式化后，先取消挂载 */
            res_sd = f_mount(NULL, SDPath, 1);
            /* 重新挂载	*/
            res_sd = f_mount(&fs, SDPath, 1);
        }
        else
        {
            PRINT_DEBUG("Formatting error");
            while (1)
                ;
        }
    }
    else if (res_sd != FR_OK)
    {
        PRINT_DEBUG("挂载文件系统失败.(%d)", res_sd);
        while (1)
            ;
    }
    else
    {
        PRINT_INFO("The file system was successfully mounted");
    }
}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSE)
 *            SYSCLK(Hz)                     = 168000000
 *            HCLK(Hz)                       = 168000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 4
 *            APB2 Prescaler                 = 2
 *            HSE Frequency(Hz)              = 8000000
 *            PLL_M                          = 25
 *            PLL_N                          = 336
 *            PLL_P                          = 2
 *            PLL_Q                          = 7
 *            VDD(V)                         = 3.3
 *            Main regulator output voltage  = Scale1 mode
 *            Flash Latency(WS)              = 5
 * @param  None
 * @retval None
 */
static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* The voltage scaling allows optimizing the power consumption when the device is
       clocked below the maximum system frequency, to update the voltage scaling value
       regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while (1)
        {
        };
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
       clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        while (1)
        {
        };
    }

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / configTICK_RATE_HZ);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

    /* STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported  */
    if (HAL_GetREVID() == 0x1001)
    {
        /* Enable the Flash prefetch */
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }
}
/*-----------------------------------------------------------*/
