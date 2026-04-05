#include <string.h>

#include "tasks_upgrade.h"

#include "debug.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "ff.h"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"

/**
 * @brief 升级状态机
 */
typedef enum
{
	UPGRADE_IDLE = 0,
	UPGRADE_START_RECEIVED,
	UPGRADE_RECEIVING_DATA,
	UPGRADE_FINISH_RECEIVED
} upgrade_state_t;

/**
 * @brief 协议解析状态
 */
typedef enum
{
	PARSE_HEADER = 0, // 正在解析包头（4字节长度）
	PARSE_DATA		  // 正在解析数据
} parse_state_t;

static FATFS fs; /*FatFs文件系统对象*/

static upgrade_state_t upgrade_state = UPGRADE_IDLE;
static FIL firmware_file;
static uint32_t total_firmware_size = 0;
static uint32_t received_size = 0;

// 处理升级开始命令
static int handle_upgrade_start(struct netconn *conn, uint8_t *data, uint32_t len)
{
	if (upgrade_state != UPGRADE_IDLE)
	{
		PRINT_DEBUG("Upgrade already in progress\n");
		return -1;
	}

	// 解析 "update start:文件大小"
	char *start_cmd = (char *)data;
	if (strncmp(start_cmd, "update start:", 13) == 0)
	{
		total_firmware_size = atoi(start_cmd + 13);
		received_size = 0;

		// 挂载SD卡
		if (f_mount(&fs, "0:", 1) != FR_OK)
		{
			PRINT_DEBUG("SD card mount failed!\n");
			return -1;
		}

		// 创建固件文件
		if (f_open(&firmware_file, "0:/firmware.bin", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
		{
			PRINT_DEBUG("Create firmware file failed!\n");
			return -1;
		}

		upgrade_state = UPGRADE_START_RECEIVED;
		PRINT_INFO("Upgrade started, total size: %lu bytes\n", total_firmware_size);

		return 0;
	}

	return -1;
}

// 处理升级数据
static int handle_upgrade_data(struct netconn *conn, uint8_t *data, uint32_t len)
{
	if (upgrade_state != UPGRADE_START_RECEIVED && upgrade_state != UPGRADE_RECEIVING_DATA)
	{
		PRINT_DEBUG("Upgrade not started\n");
		return -1;
	}

	// 写入数据到文件
	UINT bytes_written;
	if (f_write(&firmware_file, data, len, &bytes_written) != FR_OK)
	{
		PRINT_DEBUG("Write to file failed!\n");
		return -1;
	}

	if (bytes_written != len)
	{
		PRINT_DEBUG("Write incomplete: %u vs %lu\n", bytes_written, len);
		return -1;
	}

	received_size += len;
	upgrade_state = UPGRADE_RECEIVING_DATA;

	// 每接收1KB数据打印进度
	if (received_size % 1024 == 0 || received_size >= total_firmware_size)
	{
		int progress = (received_size * 100) / total_firmware_size;
		PRINT_INFO("Upgrade progress: %d%%, received: %lu/%lu bytes\n",
				   progress, received_size, total_firmware_size);
	}

	return 0;
}

// 处理升级完成命令
static int handle_upgrade_finish(struct netconn *conn, uint8_t *data, uint32_t len)
{
	if (upgrade_state != UPGRADE_RECEIVING_DATA && upgrade_state != UPGRADE_START_RECEIVED)
	{
		PRINT_DEBUG("Upgrade not in receiving state\n");
		return -1;
	}

	// 验证是否是完成命令
	if (strncmp((char *)data, "update finish", len) != 0)
	{
		PRINT_DEBUG("Invalid finish command\n");
		return -1;
	}

	// 关闭文件
	f_close(&firmware_file);

	// 验证文件大小
	if (received_size != total_firmware_size)
	{
		PRINT_DEBUG("File size mismatch: %lu vs %lu\n", received_size, total_firmware_size);
		upgrade_state = UPGRADE_IDLE;
		return -1;
	}

	upgrade_state = UPGRADE_FINISH_RECEIVED;
	PRINT_INFO("Upgrade completed successfully! Total received: %lu bytes\n", received_size);

	// 这里可以添加系统复位逻辑，让bootloader进行实际升级
	// NVIC_SystemReset();

	return 0;
}

// 处理接收到的数据包
static void process_received_packet(struct netconn *conn, uint8_t *data, uint32_t data_len)
{
	switch (upgrade_state)
	{
	case UPGRADE_IDLE:
		// 期待收到 "update start:文件大小"
		handle_upgrade_start(conn, data, data_len);
		break;

	case UPGRADE_START_RECEIVED:
	case UPGRADE_RECEIVING_DATA:
		// 检查是否是完成命令
		if (strncmp((char *)data, "update finish", data_len) == 0)
		{
			handle_upgrade_finish(conn, data, data_len);
		}
		else
		{
			// 普通数据包
			handle_upgrade_data(conn, data, data_len);
		}
		break;

	case UPGRADE_FINISH_RECEIVED:
		PRINT_DEBUG("Upgrade already finished, ignoring data\n");
		break;
	}
}

void upgrade_task(void *pvParameters)
{
	struct netconn *conn;
	struct netbuf *buf;
	void *data;
	uint16_t len;
	int ret;
	ip4_addr_t ipaddr;

	// 协议解析相关变量
	static parse_state_t parse_state = PARSE_HEADER;
	static uint32_t packet_length = 0;
	static uint32_t packet_received = 0;
	static uint8_t packet_buffer[1500]; // 足够大的缓冲区

	PRINT_INFO("Upgrade task started\n");

	while (1)
	{
		// 创建TCP连接
		conn = netconn_new(NETCONN_TCP);
		if (conn == NULL)
		{
			PRINT_DEBUG("Create conn failed!\n");
			vTaskDelay(1000);
			continue;
		}

		// 设置服务器地址
		IP4_ADDR(&ipaddr, SERVER_IP_ADDR0, SERVER_IP_ADDR1, SERVER_IP_ADDR2, SERVER_IP_ADDR3);

		// 连接服务器
		ret = netconn_connect(conn, &ipaddr, SERVER_PORT);
		if (ret != ERR_OK)
		{
			PRINT_DEBUG("Connect failed! Error: %d\n", ret);
			netconn_close(conn);
			netconn_delete(conn);
			vTaskDelay(1000);
			continue;
		}

		PRINT_INFO("Connected to upgrade server successfully!\n");

		// 重置协议解析状态
		parse_state = PARSE_HEADER;
		packet_length = 0;
		packet_received = 0;
		upgrade_state = UPGRADE_IDLE;

		// 主接收循环
		while (1)
		{
			// 接收数据
			ret = netconn_recv(conn, &buf);
			if (ret == ERR_OK)
			{
				// 获取数据指针和长度
				netbuf_data(buf, &data, &len);
				uint8_t *current_data = (uint8_t *)data;
				uint32_t remaining_len = len;

				while (remaining_len > 0)
				{
					if (parse_state == PARSE_HEADER)
					{
						// 需要4字节的长度头
						uint32_t needed = 4 - packet_received;
						uint32_t to_copy = (remaining_len < needed) ? remaining_len : needed;

						memcpy((uint8_t *)&packet_length + packet_received, current_data, to_copy);
						packet_received += to_copy;
						current_data += to_copy;
						remaining_len -= to_copy;

						if (packet_received == 4)
						{
							// 完整接收到长度头，切换到数据解析状态
							parse_state = PARSE_DATA;
							packet_received = 0;

							PRINT_DEBUG("Expecting packet of length: %lu\n", packet_length);

							if (packet_length > sizeof(packet_buffer))
							{
								PRINT_DEBUG("Packet too large: %lu\n", packet_length);
								// 跳过这个包
								parse_state = PARSE_HEADER;
								packet_length = 0;
							}
						}
					}

					if (parse_state == PARSE_DATA && remaining_len > 0)
					{
						// 接收数据部分
						uint32_t needed = packet_length - packet_received;
						uint32_t to_copy = (remaining_len < needed) ? remaining_len : needed;

						memcpy(packet_buffer + packet_received, current_data, to_copy);
						packet_received += to_copy;
						current_data += to_copy;
						remaining_len -= to_copy;

						if (packet_received == packet_length)
						{
							// 完整接收到一个数据包，进行处理
							process_received_packet(conn, packet_buffer, packet_length);

							// 重置为接收下一个包
							parse_state = PARSE_HEADER;
							packet_length = 0;
							packet_received = 0;
						}
					}
				}

				// 释放网络缓冲区
				netbuf_delete(buf);
			}
			else if (ret == ERR_CLSD)
			{
				PRINT_DEBUG("Connection closed by server\n");
				break;
			}
			else
			{
				PRINT_DEBUG("Receive error: %d\n", ret);
				break;
			}

			// 检查升级是否完成
			if (upgrade_state == UPGRADE_FINISH_RECEIVED)
			{
				PRINT_INFO("Upgrade completed, waiting for reset...\n");
				// 等待一段时间让日志输出完成
				vTaskDelay(1000);

				// 系统复位，进入bootloader升级
				// NVIC_SystemReset();

				// 如果暂时不想复位，可以重置状态
				upgrade_state = UPGRADE_IDLE;
				break;
			}

			vTaskDelay(10); // 让出CPU
		}

		// 清理连接
		netconn_close(conn);
		netconn_delete(conn);

		PRINT_INFO("Disconnected from server, waiting for reconnect...\n");
		vTaskDelay(2000);
	}
}
