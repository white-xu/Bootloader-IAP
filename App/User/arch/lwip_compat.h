#ifndef __LWIP_COMPAT_H__
#define __LWIP_COMPAT_H__

#include "elog.h"

#ifndef LWIP_LOG_TAG
#define LWIP_LOG_TAG "lwip"
#endif

#define PRINT_DEBUG(...) elog_d(LWIP_LOG_TAG, __VA_ARGS__)
#define PRINT_INFO(...)  elog_i(LWIP_LOG_TAG, __VA_ARGS__)
#define PRINT_ERR(...)   elog_e(LWIP_LOG_TAG, __VA_ARGS__)
#define PRINT_RAW(...)   elog_raw(__VA_ARGS__)

#define LED2_TOGGLE      ((void)0)

#endif
