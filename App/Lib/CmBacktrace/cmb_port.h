#ifndef __CMB_PORT_H__
#define __CMB_PORT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmb_println_impl(const char *format, ...);
uint32_t cmb_get_current_sp(void);

#ifdef __cplusplus
}
#endif

#endif /* __CMB_PORT_H__ */
