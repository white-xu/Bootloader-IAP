#ifndef __MALLOC_H
#define __MALLOC_H

#include "stm32f4xx.h"
#include "lwipopts.h"

/**
 * @brief 单条MQTT消息的最大长度
 */
#define MSG_MAX_LEN 500

/**
 * @brief 心跳保活时间，单位秒
 */
#define KEEPLIVE_TIME 50

/**
 * @brief MQTT服务质量等级枚举
 */
enum QoS
{
  QOS0 = 0, /**< 最多一次交付，不保证消息到达 */
  QOS1,     /**< 至少一次交付，保证消息到达但可能重复 */
  QOS2      /**< 恰好一次交付，保证消息准确到达一次 */
};

/**
 * @brief MQTT连接状态枚举
 */
enum MQTT_Connect
{
  Connect_OK = 0, /**< 连接成功 */
  Connect_NOK,    /**< 连接失败 */
  Connect_NOTACK  /**< 连接无响应 */
};

/**
 * @brief 发送MQTT心跳包
 */
int32_t mqtt_ping_req(int32_t sock);

/**
 * @brief 连接MQTT服务器
 */
uint8_t mqtt_connect(void);

/**
 * @brief 订阅MQTT主题
 */
int32_t mqtt_subscribe(int32_t sock, char *topic, enum QoS pos);

/**
 * @brief 发布MQTT消息
 */
int32_t mqtt_msg_publish(int32_t sock, char *topic, int8_t qos, uint8_t *msg);

/**
 * @brief 带超时的数据包读取
 */
int32_t mqtt_packet_read(int32_t sock, uint8_t *buf, int32_t buflen, uint32_t timeout);

/**
 * @brief MQTT数据包类型处理
 */
void mqtt_pktype_ctl(uint8_t packtype, uint8_t *buf, uint32_t buflen);

#endif
