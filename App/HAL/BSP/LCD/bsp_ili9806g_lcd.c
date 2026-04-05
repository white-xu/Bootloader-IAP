/**
 ******************************************************************************
 * @file    bsp_ili9806g_lcd.c
 * @version V1.0
 * @date    2013-xx-xx
 * @brief   ILI9806G液晶屏驱动
 ******************************************************************************
 * @attention
 *
 * 实验平台:野火  STM32 F407 开发板
 * 论坛    :http://www.firebbs.cn
 * 淘宝    :https://fire-stm32.taobao.com
 *
 ******************************************************************************
 */

#include "./BSP/LCD/bsp_ili9806g_lcd.h"

SRAM_HandleTypeDef SRAM_Handler;
FMC_NORSRAM_TimingTypeDef Timing;
// 根据液晶扫描方向而变化的XY像素宽度
// 调用ILI9806G_GramScan函数设置方向时会自动更改
uint16_t LCD_X_LENGTH = ILI9806G_MORE_PIXEL;
uint16_t LCD_Y_LENGTH = ILI9806G_LESS_PIXEL;

// 液晶屏扫描模式，本变量主要用于方便选择触摸屏的计算参数
// 参数可选值为0-7
// 调用ILI9806G_GramScan函数设置方向时会自动更改
// LCD刚初始化完成时会使用本默认值
uint8_t LCD_SCAN_MODE = 6;

static uint16_t CurrentTextColor = WHITE;	 // 前景色
static uint16_t CurrentBackColor = BLACK;	 // 背景色

static void ILI9806G_Write_Cmd(uint16_t usCmd);
static void ILI9806G_Write_Data(uint16_t usData);
static uint16_t ILI9806G_Read_Data(void);
static void ILI9806G_Delay(__IO uint32_t nCount);
static void ILI9806G_GPIO_Config(void);
static void ILI9806G_FSMC_Config(void);
static void ILI9806G_REG_Config(void);
static void ILI9806G_SetCursor(uint16_t usX, uint16_t usY);
static __inline void ILI9806G_FillColor(uint32_t ulAmout_Point, uint16_t usColor);
static uint16_t ILI9806G_Read_PixelData(void);

/**
 * @brief  简单延时函数
 * @param  nCount ：延时计数值
 * @retval 无
 */
static void Delay(__IO uint32_t nCount)
{
	for (; nCount != 0; nCount--)
		;
}

/**
 * @brief  向ILI9806G写入命令
 * @param  usCmd :要写入的命令（表寄存器地址）
 * @retval 无
 */
static void ILI9806G_Write_Cmd(uint16_t usCmd)
{
	*(__IO uint16_t *)(FSMC_Addr_ILI9806G_CMD) = usCmd;
}

/**
 * @brief  向ILI9806G写入数据
 * @param  usData :要写入的数据
 * @retval 无
 */
static void ILI9806G_Write_Data(uint16_t usData)
{
	*(__IO uint16_t *)(FSMC_Addr_ILI9806G_DATA) = usData;
}

/**
 * @brief  从ILI9806G读取数据
 * @param  无
 * @retval 读取到的数据
 */
static uint16_t ILI9806G_Read_Data(void)
{
	return (*(__IO uint16_t *)(FSMC_Addr_ILI9806G_DATA));
}

/**
 * @brief  用于 ILI9806G 简单延时函数
 * @param  nCount ：延时计数值
 * @retval 无
 */
static void ILI9806G_Delay(__IO uint32_t nCount)
{
	for (; nCount != 0; nCount--)
		;
}

/**
 * @brief  初始化ILI9806G的IO引脚
 * @param  无
 * @retval 无
 */
static void ILI9806G_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_Initure;

	/* Enable GPIOs clock */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_FSMC_CLK_ENABLE(); // 使能FSMC时钟

	/* Common GPIO configuration */
	GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出
	GPIO_Initure.Pull = GPIO_PULLUP;
	GPIO_Initure.Speed = GPIO_SPEED_HIGH;

	GPIO_Initure.Pin = GPIO_PIN_10;
	HAL_GPIO_Init(GPIOG, &GPIO_Initure);

	// 初始化PF11
	GPIO_Initure.Pin = GPIO_PIN_11;
	HAL_GPIO_Init(GPIOF, &GPIO_Initure);

	GPIO_Initure.Mode = GPIO_MODE_AF_PP;
	GPIO_Initure.Alternate = GPIO_AF12_FSMC; // 复用为FSMC

	// 初始化PD0,1,4,5,8,9,10,14,15
	GPIO_Initure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 |
					   GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &GPIO_Initure);

	// 初始化PE7,8,9,10,11,12,13,14,15
	GPIO_Initure.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
					   GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &GPIO_Initure);

	// 初始化PF9
	GPIO_Initure.Pin = GPIO_PIN_0 | GPIO_PIN_9;
	HAL_GPIO_Init(GPIOF, &GPIO_Initure);
}

/**
 * @brief  LCD  FSMC 模式配置
 * @param  无
 * @retval 无
 */
static void ILI9806G_FSMC_Config(void)
{
	SRAM_Handler.Instance = FMC_NORSRAM_DEVICE;
	SRAM_Handler.Extended = FMC_NORSRAM_EXTENDED_DEVICE;

	/* SRAM device configuration */
	Timing.AddressSetupTime = 0x04;
	Timing.AddressHoldTime = 0x00;
	Timing.DataSetupTime = 0x04;
	Timing.BusTurnAroundDuration = 0x00;
	Timing.CLKDivision = 0x00;
	Timing.DataLatency = 0x00;
	Timing.AccessMode = FSMC_ACCESS_MODE_B;

	SRAM_Handler.Init.NSBank = FSMC_NORSRAM_BANK3;						  // 使用NE3
	SRAM_Handler.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;	  // 地址/数据线不复用
	SRAM_Handler.Init.MemoryType = FSMC_MEMORY_TYPE_NOR;				  // NOR
	SRAM_Handler.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;	  // 16位数据宽度
	SRAM_Handler.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;	  // 是否使能突发访问,仅对同步突发存储器有效,此处未用到
	SRAM_Handler.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW; // 等待信号的极性,仅在突发模式访问下有用
	SRAM_Handler.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;	  // 存储器是在等待周期之前的一个时钟周期还是等待周期期间使能NWAIT
	SRAM_Handler.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;		  // 存储器写使能
	SRAM_Handler.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;			  // 等待使能位,此处未用到
	SRAM_Handler.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;		  // 读写使用相同的时序
	SRAM_Handler.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;  // 是否使能同步传输模式下的等待信号,此处未用到
	SRAM_Handler.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;			  // 禁止突发写
	SRAM_Handler.Init.ContinuousClock = FSMC_CONTINUOUS_CLOCK_SYNC_ASYNC;

	HAL_SRAM_Init(&SRAM_Handler, &Timing, &Timing);
}

/**
 * @brief  初始化ILI9806G寄存器
 * @param  无
 * @retval 无
 */
/**
 * @brief  初始化ILI9806G寄存器
 * @param  无
 * @retval 无
 */
static void ILI9806G_REG_Config(void)
{

	//************* Start Initial Sequence **********//
	ILI9806G_Write_Cmd(0xFF); // EXTC Command Set enable register
	ILI9806G_Write_Data(0xFF);
	ILI9806G_Write_Data(0x98);
	ILI9806G_Write_Data(0x06);

	ILI9806G_Write_Cmd(0xBA); // SPI Interface Setting
	ILI9806G_Write_Data(0x60);

	ILI9806G_Write_Cmd(0xBC); // GIP 1
	ILI9806G_Write_Data(0x01);
	ILI9806G_Write_Data(0x0E);
	ILI9806G_Write_Data(0x61);
	ILI9806G_Write_Data(0xFB);
	ILI9806G_Write_Data(0x10);
	ILI9806G_Write_Data(0x10);
	ILI9806G_Write_Data(0x0B);
	ILI9806G_Write_Data(0x0F);
	ILI9806G_Write_Data(0x2E);
	ILI9806G_Write_Data(0x73);
	ILI9806G_Write_Data(0xFF);
	ILI9806G_Write_Data(0xFF);
	ILI9806G_Write_Data(0x0E);
	ILI9806G_Write_Data(0x0E);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x03);
	ILI9806G_Write_Data(0x66);
	ILI9806G_Write_Data(0x63);
	ILI9806G_Write_Data(0x01);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);

	ILI9806G_Write_Cmd(0xBD); // GIP 2
	ILI9806G_Write_Data(0x01);
	ILI9806G_Write_Data(0x23);
	ILI9806G_Write_Data(0x45);
	ILI9806G_Write_Data(0x67);
	ILI9806G_Write_Data(0x01);
	ILI9806G_Write_Data(0x23);
	ILI9806G_Write_Data(0x45);
	ILI9806G_Write_Data(0x67);

	ILI9806G_Write_Cmd(0xBE); // GIP 3
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x21);
	ILI9806G_Write_Data(0xAB);
	ILI9806G_Write_Data(0x60);
	ILI9806G_Write_Data(0x22);
	ILI9806G_Write_Data(0x22);
	ILI9806G_Write_Data(0x22);
	ILI9806G_Write_Data(0x22);
	ILI9806G_Write_Data(0x22);

	ILI9806G_Write_Cmd(0xC7); // Vcom
	ILI9806G_Write_Data(0x6F);

	ILI9806G_Write_Cmd(0xED); // EN_volt_reg
	ILI9806G_Write_Data(0x7F);
	ILI9806G_Write_Data(0x0F);
	ILI9806G_Write_Data(0x00);

	ILI9806G_Write_Cmd(0xC0); // Power Control 1
	ILI9806G_Write_Data(0x37);
	ILI9806G_Write_Data(0x0B);
	ILI9806G_Write_Data(0x0A);

	ILI9806G_Write_Cmd(0xFC); // LVGL
	ILI9806G_Write_Data(0x0A);

	ILI9806G_Write_Cmd(0xDF); // Engineering Setting
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x20);

	ILI9806G_Write_Cmd(0xF3); // DVDD Voltage Setting
	ILI9806G_Write_Data(0x74);

	ILI9806G_Write_Cmd(0xB4); // Display Inversion Control
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x00);

	ILI9806G_Write_Cmd(0xF7); // 480x800
	ILI9806G_Write_Data(0x8A);

	ILI9806G_Write_Cmd(0xB1); // Frame Rate
	ILI9806G_Write_Data(0x00);
	ILI9806G_Write_Data(0x12);
	ILI9806G_Write_Data(0x13);

	ILI9806G_Write_Cmd(0xF2); // Panel Timing Control
	ILI9806G_Write_Data(0x80);
	ILI9806G_Write_Data(0x5B);
	ILI9806G_Write_Data(0x40);
	ILI9806G_Write_Data(0x28);

	ILI9806G_Write_Cmd(0xC1); // Power Control 2
	ILI9806G_Write_Data(0x17);
	ILI9806G_Write_Data(0x7D);
	ILI9806G_Write_Data(0x7A);
	ILI9806G_Write_Data(0x20);

	ILI9806G_Write_Cmd(0xE0);
	ILI9806G_Write_Data(0x00); // P1
	ILI9806G_Write_Data(0x11); // P2
	ILI9806G_Write_Data(0x1C); // P3
	ILI9806G_Write_Data(0x0E); // P4
	ILI9806G_Write_Data(0x0F); // P5
	ILI9806G_Write_Data(0x0C); // P6
	ILI9806G_Write_Data(0xC7); // P7
	ILI9806G_Write_Data(0x06); // P8
	ILI9806G_Write_Data(0x06); // P9
	ILI9806G_Write_Data(0x0A); // P10
	ILI9806G_Write_Data(0x10); // P11
	ILI9806G_Write_Data(0x12); // P12
	ILI9806G_Write_Data(0x0A); // P13
	ILI9806G_Write_Data(0x10); // P14
	ILI9806G_Write_Data(0x02); // P15
	ILI9806G_Write_Data(0x00); // P16

	ILI9806G_Write_Cmd(0xE1);
	ILI9806G_Write_Data(0x00); // P1
	ILI9806G_Write_Data(0x12); // P2
	ILI9806G_Write_Data(0x18); // P3
	ILI9806G_Write_Data(0x0C); // P4
	ILI9806G_Write_Data(0x0F); // P5
	ILI9806G_Write_Data(0x0A); // P6
	ILI9806G_Write_Data(0x77); // P7
	ILI9806G_Write_Data(0x06); // P8
	ILI9806G_Write_Data(0x07); // P9
	ILI9806G_Write_Data(0x0A); // P10
	ILI9806G_Write_Data(0x0E); // P11
	ILI9806G_Write_Data(0x0B); // P12
	ILI9806G_Write_Data(0x10); // P13
	ILI9806G_Write_Data(0x1D); // P14
	ILI9806G_Write_Data(0x17); // P15
	ILI9806G_Write_Data(0x00); // P16

	ILI9806G_Write_Cmd(0x35); // Tearing Effect ON
	ILI9806G_Write_Data(0x00);

	ILI9806G_Write_Cmd(0x3A);
	ILI9806G_Write_Data(0x55);

	ILI9806G_Write_Cmd(0x11); // Exit Sleep
	DEBUG_DELAY();
	ILI9806G_Write_Cmd(0x29); // Display On
}

/**
 * @brief  ILI9806G初始化函数，如果要用到lcd，一定要调用这个函数
 * @param  无
 * @retval 无
 */
void ILI9806G_Init(void)
{
	ILI9806G_GPIO_Config();
	ILI9806G_FSMC_Config();

	ILI9806G_Rst();
	ILI9806G_REG_Config();

	// 设置默认扫描方向，其中 6 模式为大部分液晶例程的默认显示方向
	ILI9806G_GramScan(LCD_SCAN_MODE);

	ILI9806G_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH); /* 清屏，显示全黑 */
	ILI9806G_BackLed_Control(ENABLE);				  // 点亮LCD背光灯
}

/**
 * @brief  ILI9806G背光LED控制
 * @param  enumState ：决定是否使能背光LED
 *   该参数为以下值之一：
 *     @arg ENABLE :使能背光LED
 *     @arg DISABLE :禁用背光LED
 * @retval 无
 */
void ILI9806G_BackLed_Control(FunctionalState enumState)
{
	if (enumState)
	{
		digitalH(GPIOF, GPIO_PIN_9);
	}
	else
	{
		digitalL(GPIOF, GPIO_PIN_9);
	}
}

/**
 * @brief  ILI9806G 软件复位
 * @param  无
 * @retval 无
 */
void ILI9806G_Rst(void)
{
	digitalL(GPIOF, GPIO_PIN_11); // 低电平复位

	ILI9806G_Delay(30000);

	digitalH(GPIOF, GPIO_PIN_11);

	ILI9806G_Delay(30000);
}

/**
 * @brief  设置ILI9806G的GRAM的扫描方向
 * @param  ucOption ：选择GRAM的扫描方向
 *     @arg 0-7 :参数可选值为0-7这八个方向
 *
 *	！！！其中0、3、5、6 模式适合从左至右显示文字，
 *				不推荐使用其它模式显示文字	其它模式显示文字会有镜像效果
 *
 *	其中0、2、4、6 模式的X方向像素为480，Y方向像素为854
 *	其中1、3、5、7 模式下X方向像素为854，Y方向像素为480
 *
 *	其中 6 模式为大部分液晶例程的默认显示方向
 *	其中 3 模式为摄像头例程使用的方向
 *	其中 0 模式为BMP图片显示例程使用的方向
 *
 * @retval 无
 * @note  坐标图例：A表示向上，V表示向下，<表示向左，>表示向右
					X表示X轴，Y表示Y轴

------------------------------------------------------------
模式0：				.		模式1：		.	模式2：			.	模式3：
					A		.					A		.		A					.		A
					|		.					|		.		|					.		|
					Y		.					X		.		Y					.		X
					0		.					1		.		2					.		3
	<--- X0 o		.	<----Y1	o		.		o 2X--->  .		o 3Y--->
------------------------------------------------------------
模式4：				.	模式5：			.	模式6：			.	模式7：
	<--- X4 o		.	<--- Y5 o		.		o 6X--->  .		o 7Y--->
					4		.					5		.		6					.		7
					Y		.					X		.		Y					.		X
					|		.					|		.		|					.		|
					V		.					V		.		V					.		V
---------------------------------------------------------
											 LCD屏示例
								|-----------------|
								|			野火Logo		|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|-----------------|
								屏幕正面（宽480，高854）

 *******************************************************/
void ILI9806G_GramScan(uint8_t ucOption)
{
	// 参数检查，只可输入0-7
	if (ucOption > 7)
		return;

	// 根据模式更新LCD_SCAN_MODE的值，主要用于触摸屏选择计算参数
	LCD_SCAN_MODE = ucOption;

	// 根据模式更新XY方向的像素宽度
	if (ucOption % 2 == 0)
	{
		// 0 2 4 6模式下X方向像素宽度为480，Y方向为854
		LCD_X_LENGTH = ILI9806G_LESS_PIXEL;
		LCD_Y_LENGTH = ILI9806G_MORE_PIXEL;
	}
	else
	{
		// 1 3 5 7模式下X方向像素宽度为854，Y方向为480
		LCD_X_LENGTH = ILI9806G_MORE_PIXEL;
		LCD_Y_LENGTH = ILI9806G_LESS_PIXEL;
	}

	// 0x36命令参数的高3位可用于设置GRAM扫描方向
	ILI9806G_Write_Cmd(0x36);
	ILI9806G_Write_Data(0x00 | (ucOption << 5)); // 根据ucOption的值设置LCD参数，共0-7种模式
	ILI9806G_Write_Cmd(CMD_SetCoordinateX);
	ILI9806G_Write_Data(0x00);							   /* x 起始坐标高8位 */
	ILI9806G_Write_Data(0x00);							   /* x 起始坐标低8位 */
	ILI9806G_Write_Data(((LCD_X_LENGTH - 1) >> 8) & 0xFF); /* x 结束坐标高8位 */
	ILI9806G_Write_Data((LCD_X_LENGTH - 1) & 0xFF);		   /* x 结束坐标低8位 */

	ILI9806G_Write_Cmd(CMD_SetCoordinateY);
	ILI9806G_Write_Data(0x00);							   /* y 起始坐标高8位 */
	ILI9806G_Write_Data(0x00);							   /* y 起始坐标低8位 */
	ILI9806G_Write_Data(((LCD_Y_LENGTH - 1) >> 8) & 0xFF); /* y 结束坐标高8位 */
	ILI9806G_Write_Data((LCD_Y_LENGTH - 1) & 0xFF);		   /* y 结束坐标低8位 */

	/* write gram start */
	ILI9806G_Write_Cmd(CMD_SetPixel);
}

/**
 * @brief  在ILI9806G显示器上开辟一个窗口
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @retval 无
 */
void ILI9806G_OpenWindow(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight)
{
	ILI9806G_Write_Cmd(CMD_SetCoordinateX); /* 设置X坐标 */
	ILI9806G_Write_Data(usX >> 8);			/* 先高8位，然后低8位 */
	ILI9806G_Write_Data(usX & 0xff);		/* 设置起始点和结束点*/
	ILI9806G_Write_Data((usX + usWidth - 1) >> 8);
	ILI9806G_Write_Data((usX + usWidth - 1) & 0xff);

	ILI9806G_Write_Cmd(CMD_SetCoordinateY); /* 设置Y坐标*/
	ILI9806G_Write_Data(usY >> 8);
	ILI9806G_Write_Data(usY & 0xff);
	ILI9806G_Write_Data((usY + usHeight - 1) >> 8);
	ILI9806G_Write_Data((usY + usHeight - 1) & 0xff);
}

/**
 * @brief  设定ILI9806G的光标坐标
 * @param  usX ：在特定扫描方向下光标的X坐标
 * @param  usY ：在特定扫描方向下光标的Y坐标
 * @retval 无
 */
static void ILI9806G_SetCursor(uint16_t usX, uint16_t usY)
{
	ILI9806G_OpenWindow(usX, usY, 1, 1);
}

/**
 * @brief  在ILI9806G显示器上以某一颜色填充像素点
 * @param  ulAmout_Point ：要填充颜色的像素点的总数目
 * @param  usColor ：颜色
 * @retval 无
 */
static __inline void ILI9806G_FillColor(uint32_t ulAmout_Point, uint16_t usColor)
{
	uint32_t i = 0;

	/* memory write */
	ILI9806G_Write_Cmd(CMD_SetPixel);

	for (i = 0; i < ulAmout_Point; i++)
		ILI9806G_Write_Data(usColor);
}

/**
 * @brief  对ILI9806G显示器的某一窗口以某种颜色进行清屏
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_Clear(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight)
{
	ILI9806G_OpenWindow(usX, usY, usWidth, usHeight);

	ILI9806G_FillColor(usWidth * usHeight, CurrentBackColor);
}

/**
 * @brief  对ILI9806G显示器的某一点以某种颜色进行填充
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_SetPointPixel(uint16_t usX, uint16_t usY)
{
	if ((usX < LCD_X_LENGTH) && (usY < LCD_Y_LENGTH))
	{
		ILI9806G_SetCursor(usX, usY);

		ILI9806G_FillColor(1, CurrentTextColor);
	}
}

void ILI9806G_SetColorPointPixel(uint16_t usX, uint16_t usY, uint16_t color)
{
	if ((usX < LCD_X_LENGTH) && (usY < LCD_Y_LENGTH))
	{
		ILI9806G_SetCursor(usX, usY);

		ILI9806G_FillColor(1, color);
	}
}

/**
 * @brief  读取ILI9806G GRAN 的一个像素数据
 * @param  无
 * @retval 像素数据
 */
static uint16_t ILI9806G_Read_PixelData(void)
{
	uint16_t usR = 0, usG = 0, usB = 0;

	ILI9806G_Write_Cmd(0x2E); /* 读数据 */

	usR = ILI9806G_Read_Data(); /*FIRST READ OUT DUMMY DATA*/

	usR = ILI9806G_Read_Data(); /*READ OUT RED DATA  */
	usB = ILI9806G_Read_Data(); /*READ OUT BLUE DATA*/
	usG = ILI9806G_Read_Data(); /*READ OUT GREEN DATA*/

	return (((usR >> 11) << 11) | ((usG >> 10) << 5) | (usB >> 11));
}

/**
 * @brief  获取 ILI9806G 显示器上某一个坐标点的像素数据
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @retval 像素数据
 */
uint16_t ILI9806G_GetPointPixel(uint16_t usX, uint16_t usY)
{
	uint16_t usPixelData;

	ILI9806G_SetCursor(usX, usY);

	usPixelData = ILI9806G_Read_PixelData();

	return usPixelData;
}

/*********************end of file*************************/
