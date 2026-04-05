#ifndef __TASKS_MQTT_H
#define __TASKS_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief MQTT主题名称的最大长度
 */
#define MSG_TOPIC_LEN 50

/**
 * @brief 服务器域名
 */
#define HOST_NAME "k12fu2bQpOt.iot-as-mqtt.cn-shanghai.aliyuncs.com"

/**
 * @brief 服务器端口号（TCP连接）
 */
#define HOST_PORT 1883

/**
 * @brief 订阅和发布的主题
 */
#define TOPIC "/k12fu2bQpOt/device_wuhan/user/test"

void mqtt_task(void *pvParameters);
void mqtt_send_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
