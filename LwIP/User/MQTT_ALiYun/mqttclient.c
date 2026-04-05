#include "mqttclient.h"
#include "transport.h"
#include "MQTTPacket.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "sockets.h"

#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/api.h"

#include "lwip/sockets.h"

#include "cJSON_Process.h"
#include "bsp_dht11.h"

/**
 * @brief 客户端ID
 */
#define CLIENT_ID "1|securemode=3,signmethod=hmacsha1|"

/**
 * @brief MQTT协议版本号
 */
#define MQTT_VERSION 4

/**
 * @brief 用户名
 */
#define USER_NAME "device_wuhan&k12fu2bQpOt"

/**
 * @brief 密码/秘钥
 */
#define PASSWORD "8510DABDEB2CAF26AB4E3BD05FA24641ADE1D83E"

/**
 * @brief MQTT主题名称的最大长度
 */
#define MSG_TOPIC_LEN 50

/**
 * @brief MQTT消息结构体（协议层）
 */
typedef struct __MQTTMessage
{
  uint32_t qos;       /**< 服务质量等级 */
  uint8_t retained;   /**< 保留标志，服务器是否保存消息 */
  uint8_t dup;        /**< 重复发送标志 */
  uint16_t id;        /**< 消息ID（用于QoS1和QoS2） */
  uint8_t type;       /**< MQTT数据包类型 */
  void *payload;      /**< 消息负载数据指针 */
  int32_t payloadlen; /**< 消息负载长度 */
} MQTTMessage;

/**
 * @brief 用户接收消息结构体（应用层）
 */
typedef struct __MQTT_MSG
{
  uint8_t msgqos;               /**< 消息质量等级 */
  uint8_t msg[MSG_MAX_LEN];     /**< 消息内容缓冲区 */
  uint32_t msglenth;            /**< 实际消息长度 */
  uint8_t topic[MSG_TOPIC_LEN]; /**< 主题名称缓冲区 */
  uint16_t packetid;            /**< 数据包ID */
  uint8_t valid;                /**< 消息有效性标志：0-无效，1-有效 */
} MQTT_USER_MSG;

// 定义用户消息结构体
MQTT_USER_MSG mqtt_user_msg;

/**
 * @brief 连接MQTT服务器
 */
uint8_t mqtt_connect(void)
{
  uint8_t buf[200];
  int buflen = sizeof(buf);
  int len = 0;

  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.clientID.cstring = CLIENT_ID;      // 设置客户端ID
  data.keepAliveInterval = KEEPLIVE_TIME; // 设置保活时间间隔(秒)
  data.username.cstring = USER_NAME;      // 设置用户名(认证)
  data.password.cstring = PASSWORD;       // 设置密码(认证)
  data.MQTTVersion = MQTT_VERSION;        // 设置MQTT协议版本
  data.cleansession = 1;                  // 设置清理会话标志: 1=清理会话，0=保持会话

  // 组装消息
  len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);
  // 发送消息
  transport_sendPacketBuffer(buf, len);

  /* 等待连接响应 */
  if (MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
  {
    /*解析CONNACK数据包*/
    unsigned char sessionPresent, connack_rc;
    if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
    {
      PRINT_DEBUG("无法连接，错误代码是: %d！\n", connack_rc);
      return Connect_NOK;
    }
    else
    {
      PRINT_DEBUG("用户名与秘钥验证成功，MQTT连接成功！\n");
      return Connect_OK;
    }
  }
  else
    PRINT_DEBUG("MQTT连接无响应！\n");
  return Connect_NOTACK;
}

/************************************************************************
** 函数名称: mqtt_ping_req
** 函数功能: 发送MQTT心跳包
** 入口参数: 无
** 出口参数: >=0:发送成功 <0:发送失败
** 备    注:
************************************************************************/
int32_t mqtt_ping_req(int32_t sock)
{
  int32_t len;
  uint8_t buf[200];
  int32_t buflen = sizeof(buf);

  fd_set readfd;
  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  FD_ZERO(&readfd);
  FD_SET(sock, &readfd);

  len = MQTTSerialize_pingreq(buf, buflen);
  transport_sendPacketBuffer(buf, len);

  /*等待可读事件*/
  if (select(sock + 1, &readfd, NULL, NULL, &tv) == 0)
    return -1;  /*超时*/

  if (FD_ISSET(sock, &readfd) == 0)
    return -2;  /*非对应事件*/

  if (MQTTPacket_read(buf, buflen, transport_getdata) != PINGRESP)
    return -3;

  return 0;
}

/************************************************************************
** 函数名称: mqtt_subscribe
** 函数功能: 订阅消息
** 入口参数: int32_t sock：套接字
**           int8_t *topic：主题
**           enum QoS pos：消息质量
** 出口参数: >=0:发送成功 <0:发送失败
** 备    注:
************************************************************************/
int32_t mqtt_subscribe(int32_t sock, char *topic, enum QoS pos)
{
  static uint32_t PacketID = 0;
  uint16_t packetidbk = 0;
  int32_t conutbk = 0;
  uint8_t buf[100];
  int32_t buflen = sizeof(buf);
  MQTTString topicString = MQTTString_initializer;
  int32_t len;
  int32_t req_qos, qosbk;

  fd_set readfd;
  struct timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  FD_ZERO(&readfd);
  FD_SET(sock, &readfd);

  topicString.cstring = (char *)topic;
  req_qos = pos;
  len = MQTTSerialize_subscribe(buf, buflen, 0, PacketID++, 1, &topicString, &req_qos);
  if (transport_sendPacketBuffer(buf, len) < 0)
    return -1;

  /*等待可读事件*/
  if (select(sock + 1, &readfd, NULL, NULL, &tv) == 0)
    return -2;  /*超时*/
  if (FD_ISSET(sock, &readfd) == 0)
    return -3;  /*非对应事件*/

  // 等待订阅返回--未收到订阅返回
  if (MQTTPacket_read(buf, buflen, transport_getdata) != SUBACK)
    return -4;

  // 拆订阅回应包
  if (MQTTDeserialize_suback(&packetidbk, 1, &conutbk, &qosbk, buf, buflen) != 1)
    return -5;

  // 检测返回数据的正确性
  if ((qosbk == 0x80) || (packetidbk != (PacketID - 1)))
    return -6;

  // 订阅成功
  return 0;
}

/************************************************************************
** 函数名称: GetNextPackID
** 函数功能: 产生下一个数据包ID
** 入口参数: 无
** 出口参数: uint16_t packetid:产生的ID
** 备    注:
************************************************************************/
static uint16_t GetNextPackID(void)
{
  static uint16_t pubpacketid = 0;
  return pubpacketid++;
}

/************************************************************************
** 函数名称: WaitForPacket
** 函数功能: 等待特定的数据包
** 入口参数: int32_t sock:网络描述符
**           uint8_t packettype:包类型
**           uint8_t times:等待次数
** 出口参数: >=0:等到了特定的包 <0:没有等到特定的包
** 备    注:
************************************************************************/
static int32_t WaitForPacket(int32_t sock, uint8_t packettype, uint8_t times)
{
 int32_t type;
 uint8_t buf[MSG_MAX_LEN];
 uint8_t n = 0;
 int32_t buflen = sizeof(buf);
 do
 {
   // 读取数据包
   type = mqtt_packet_read(sock, buf, buflen, 2);
   if (type != -1)
     mqtt_pktype_ctl(type, buf, buflen);
   n++;
 } while ((type != packettype) && (n < times));
 // 收到期望的包
 if (type == packettype)
   return 0;
 else
   return -1;
}

/************************************************************************
** 函数名称: mqtt_msg_publish
** 函数功能: 用户推送消息
** 入口参数: MQTT_USER_MSG  *msg：消息结构体指针
** 出口参数: >=0:发送成功 <0:发送失败
** 备    注:
************************************************************************/
int32_t mqtt_msg_publish(int32_t sock, char *topic, int8_t qos, uint8_t *msg)
{
  int8_t retained = 0; // 保留标志位
  uint32_t msg_len;    // 数据长度
  uint8_t buf[MSG_MAX_LEN];
  int32_t buflen = sizeof(buf), len;
  MQTTString topicString = MQTTString_initializer;
  uint16_t packid = 0, packetidbk;

  // 填充主题
  topicString.cstring = (char *)topic;

  // 填充数据包ID
  if ((qos == QOS1) || (qos == QOS2))
  {
    packid = GetNextPackID();
  }
  else
  {
    qos = QOS0;
    retained = 0;
    packid = 0;
  }

  msg_len = strlen((char *)msg_len);

  // 推送消息
  len = MQTTSerialize_publish(buf, buflen, 0, qos, retained, packid, topicString, (unsigned char *)msg, msg_len);
  if (len <= 0)
    return -1;
  if (transport_sendPacketBuffer(buf, len) < 0)
    return -2;

  // 质量等级0，不需要返回
  if (qos == QOS0)
  {
    return 0;
  }

  // 等级1
  if (qos == QOS1)
  {
    // 等待PUBACK
    if (WaitForPacket(sock, PUBACK, 5) < 0)
      return -3;
    return 1;
  }
  // 等级2
  if (qos == QOS2)
  {
    // 等待PUBREC
    if (WaitForPacket(sock, PUBREC, 5) < 0)
      return -3;
    // 发送PUBREL
    len = MQTTSerialize_pubrel(buf, buflen, 0, packetidbk);
    if (len == 0)
      return -4;
    if (transport_sendPacketBuffer(buf, len) < 0)
      return -6;
    // 等待PUBCOMP
    if (WaitForPacket(sock, PUBREC, 5) < 0)
      return -7;
    return 2;
  }
  // 等级错误
  return -8;
}

/************************************************************************
** 函数名称: mqtt_packet_read
** 函数功能: 阻塞读取MQTT数据
** 入口参数: int32_t sock:网络描述符
**           uint8_t *buf:数据缓存区
**           int32_t buflen:缓冲区大小
**           uint32_t timeout:超时时间--0-表示直接查询，没有数据立即返回
** 出口参数: -1：错误,其他--包类型
** 备    注:
************************************************************************/
int32_t mqtt_packet_read(int32_t sock, uint8_t *buf, int32_t buflen, uint32_t timeout)
{
  fd_set readfd;
  struct timeval tv;
  if (timeout != 0)
  {
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    // 等待可读事件--等待超时
    if (select(sock + 1, &readfd, NULL, NULL, &tv) == 0)
      return -1;
    // 有可读事件--没有可读事件
    if (FD_ISSET(sock, &readfd) == 0)
      return -1;
  }
  // 读取TCP/IP事件
  return MQTTPacket_read(buf, buflen, transport_getdata);
}

/************************************************************************
** 函数名称: UserMsgCtl
** 函数功能: 用户消息处理函数
** 入口参数: MQTT_USER_MSG  *msg：消息结构体指针
** 出口参数: 无
** 备    注:
************************************************************************/
static void UserMsgCtl(MQTT_USER_MSG *msg)
{
  // 这里处理数据只是打印，用户可以在这里添加自己的处理方式
  PRINT_INFO("*****收到订阅的消息！******\n");
  // 返回后处理消息
  if (msg->msglenth > 2) // 只有当消息长度大于2 "{}" 的时候才去处理它
  {
    switch (msg->msgqos)
    {
    case 0:
      PRINT_INFO("MQTT>>消息质量：QoS0\n");
      break;
    case 1:
      PRINT_INFO("MQTT>>消息质量：QoS1\n");
      break;
    case 2:
      PRINT_INFO("MQTT>>消息质量：QoS2\n");
      break;
    default:
      PRINT_INFO("MQTT>>错误的消息质量\n");
      break;
    }
    PRINT_INFO("MQTT>>消息主题：%s\n", msg->topic);
    PRINT_INFO("MQTT>>消息类容：%s\n", msg->msg);
    PRINT_INFO("MQTT>>消息长度：%d\n", msg->msglenth);

    // Proscess(msg->msg);
  }
  // 处理完后销毁数据
  msg->valid = 0;
}

/************************************************************************
** 函数名称: deliverMessage
** 函数功能: 接受服务器发来的消息
** 入口参数: MQTTMessage *msg:MQTT消息结构体
**           MQTT_USER_MSG *mqtt_user_msg:用户接受结构体
**           MQTTString  *TopicName:主题
** 出口参数: 无
** 备    注:
************************************************************************/
static void deliverMessage(MQTTString *TopicName, MQTTMessage *msg, MQTT_USER_MSG *mqtt_user_msg)
{
 // 消息质量
 mqtt_user_msg->msgqos = msg->qos;
 // 保存消息
 memcpy(mqtt_user_msg->msg, msg->payload, msg->payloadlen);
 mqtt_user_msg->msg[msg->payloadlen] = 0;
 // 保存消息长度
 mqtt_user_msg->msglenth = msg->payloadlen;
 // 消息主题
 memcpy((char *)mqtt_user_msg->topic, TopicName->lenstring.data, TopicName->lenstring.len);
 mqtt_user_msg->topic[TopicName->lenstring.len] = 0;
 // 消息ID
 mqtt_user_msg->packetid = msg->id;
 // 标明消息合法
 mqtt_user_msg->valid = 1;
}

/************************************************************************
** 函数名称: mqtt_pktype_ctl
** 函数功能: 根据包类型进行处理
** 入口参数: uint8_t packtype:包类型
** 出口参数: 无
** 备    注:
************************************************************************/
void mqtt_pktype_ctl(uint8_t packtype, uint8_t *buf, uint32_t buflen)
{
  MQTTMessage msg;
  int32_t rc;
  MQTTString receivedTopic;
  uint32_t len;

  switch (packtype)
  {
  case PUBLISH:
    if (MQTTDeserialize_publish(&msg.dup, (int *)&msg.qos, &msg.retained, &msg.id, &receivedTopic,
                                (unsigned char **)&msg.payload, &msg.payloadlen, buf, buflen) != 1)
      return;

    deliverMessage(&receivedTopic, &msg, &mqtt_user_msg);   /*将相关字段保存到mqtt_user_msg中*/

    if (msg.qos == QOS0)
    {
      UserMsgCtl(&mqtt_user_msg);
      return;
    }

    if (msg.qos == QOS1)
    {
      len = MQTTSerialize_puback(buf, buflen, mqtt_user_msg.packetid);
      if (len == 0)
        return;

      if (transport_sendPacketBuffer(buf, len) < 0)
        return;

      UserMsgCtl(&mqtt_user_msg);
      return;
    }

    if (msg.qos == QOS2)    /*阿里云不能发送QOS2等级的数据*/
    {
      len = MQTTSerialize_ack(buf, buflen, PUBREC, 0, mqtt_user_msg.packetid);
      if (len == 0)
        return;

      transport_sendPacketBuffer(buf, len);
    }
    break;
  case PUBREL:
    // 解析包数据，必须包ID相同才可以
    rc = MQTTDeserialize_ack(&msg.type, &msg.dup, &msg.id, buf, buflen);
    if ((rc != 1) || (msg.type != PUBREL) || (msg.id != mqtt_user_msg.packetid))
      return;
    // 收到PUBREL，需要处理并抛弃数据
    if (mqtt_user_msg.valid == 1)
    {
      // 返回后处理消息
      UserMsgCtl(&mqtt_user_msg);
    }
    // 串行化PUBCMP消息
    len = MQTTSerialize_pubcomp(buf, buflen, msg.id);
    if (len == 0)
      return;
    // 发送返回--PUBCOMP
    transport_sendPacketBuffer(buf, len);
    break;
  case PUBACK: // 等级1客户端推送数据后，服务器返回
    break;
  case PUBREC: // 等级2客户端推送数据后，服务器返回
    break;
  case PUBCOMP: // 等级2客户端推送PUBREL后，服务器返回
    break;
  default:
    break;
  }
}
