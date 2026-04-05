#include "tasks_mqtt.h"
#include "debug.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/sockets.h"

#include "MQTTPacket.h"

#include "MQTT_ALiYun/mqttclient.h"
#include "MQTT_ALiYun/transport.h"
#include "MQTT_ALiYun/cJSON_Process.h"

extern void TCPIP_Init(void);

static int32_t MQTT_Socket = 0;

/**
 * @brief 使用mqtt协议连接阿里云平台，订阅消息
 */
static void mqtt_start_client(void)
{
  char *host_ip;

#if LWIP_DNS
  ip4_addr_t dns_ip;
  netconn_gethostbyname(HOST_NAME, &dns_ip);
  host_ip = ip_ntoa(&dns_ip);
  PRINT_INFO("host name : %s , host_ip : %s\n", HOST_NAME, host_ip);
#else
  host_ip = HOST_NAME;
#endif

MQTT_START:
  PRINT_INFO("1.开始连接对应云平台的服务器...\n");
  PRINT_INFO("服务器IP地址：%s，端口号：%0d！\n", host_ip, HOST_PORT);

  /*连接服务器*/
  while (1)
  {
    MQTT_Socket = transport_open((int8_t *)host_ip, HOST_PORT);
    if (MQTT_Socket >= 0)
    {
      PRINT_DEBUG("连接云平台服务器成功！\n");
      break;
    }
    PRINT_DEBUG("连接云平台服务器失败，等待3秒再尝试重新连接！\n");
    vTaskDelay(3000);
  }

  /*登录*/
  PRINT_INFO("2.MQTT用户名与秘钥验证登陆...\n");
  if (mqtt_connect() != Connect_OK)
  {
    PRINT_DEBUG("MQTT用户名与秘钥验证登陆失败...\n");
    transport_close();
    goto MQTT_START;
  }

  /*订阅消息*/
  PRINT_INFO("3.开始订阅消息...\n");
  if (mqtt_subscribe(MQTT_Socket, (char *)TOPIC, QOS1) < 0)
  {
    // 重连服务器
    PRINT_DEBUG("客户端订阅消息失败...\n");
    // 关闭链接
    transport_close();
    goto MQTT_START;
  }
}

/**
 * @brief mqtt任务
 */
void mqtt_task(void *pvParameters)
{
  PRINT_INFO("初始化LwIP协议");
  TCPIP_Init();

  uint32_t curtick;
  uint8_t no_mqtt_msg_exchange = 1;
  uint8_t buf[MSG_MAX_LEN];
  int32_t buflen = sizeof(buf);
  int32_t type;

  fd_set readfd;
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 10;

MQTT_START:
  mqtt_start_client();

  curtick = xTaskGetTickCount(); /*获取当前滴答，作为心跳包起始时间*/

  while (1)
  {
    no_mqtt_msg_exchange = 1;

    FD_ZERO(&readfd);
    FD_SET(MQTT_Socket, &readfd);
    select(MQTT_Socket + 1, &readfd, NULL, NULL, &tv);
    if (FD_ISSET(MQTT_Socket, &readfd) != 0)
    {
      type = mqtt_packet_read(MQTT_Socket, buf, buflen, 50);
      if (type != -1)
      {
        mqtt_pktype_ctl(type, buf, buflen);

        no_mqtt_msg_exchange = 0;
        curtick = xTaskGetTickCount();
      }
    }

    /*发送PING保活命令*/
    if ((xTaskGetTickCount() - curtick) > (KEEPLIVE_TIME / 2 * 1000))
    {
      curtick = xTaskGetTickCount();
      if (no_mqtt_msg_exchange == 0)
        continue;

      if (mqtt_ping_req(MQTT_Socket) < 0)
      {
        PRINT_DEBUG("发送保持活性ping失败....\n");
        goto CLOSE;
      }

      PRINT_DEBUG("发送保持活性ping作为心跳成功....\n");
      no_mqtt_msg_exchange = 0;
    }
  }

CLOSE:
  transport_close();
  goto MQTT_START;
}

static uint8_t send_data_flag = 1;

/**
 * @brief mqtt发送任务
 */
void mqtt_send_task(void *pvParameters)
{
  uint8_t res;
  int8_t ret;

  cJSON *cJSON_Data = NULL;
  cJSON_Data = cJSON_Data_Init();
  double a = 1.3f, b = 4.2f;

  /*先等连接上阿里云后再发送数据*/
  vTaskDelay(pdMS_TO_TICKS(3000));
  vTaskDelay(pdMS_TO_TICKS(3000));

  while (1)
  {
    if (send_data_flag)
    {
      res = cJSON_Update(cJSON_Data, TEMP_NUM, &a);
      res = cJSON_Update(cJSON_Data, HUM_NUM, &b);

      if (UPDATE_SUCCESS == res)
      {
        char *p = cJSON_Print(cJSON_Data);
        PRINT_INFO("发送到阿里云的测试数据：");
        PRINT_INFO("%s", p);
        ret = mqtt_msg_publish(MQTT_Socket, (char *)TOPIC, QOS0, (uint8_t *)p);
        if(ret < 0)
          PRINT_DEBUG("发送数据到阿里云失败\n");
        vPortFree(p);
        p = NULL;
      }
      else
        PRINT_DEBUG("update fail\n");

      send_data_flag = 0;
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(3000));
      vTaskDelay(pdMS_TO_TICKS(3000));
      send_data_flag = 1;
    }
  }
}
