#ifndef __TASKS_UPGRADE_H
#define __TASKS_UPGRADE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#define SERVER_IP_ADDR0             192
#define SERVER_IP_ADDR1             168
#define SERVER_IP_ADDR2             137
#define SERVER_IP_ADDR3               1

#define SERVER_PORT                8080

void upgrade_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
