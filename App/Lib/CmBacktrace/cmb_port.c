#include "cmb_port.h"

#include <stdarg.h>
#include <stdio.h>

extern void elog_port_output(const char *log, size_t size);

void cmb_println_impl(const char *format, ...)
{
    char output_buf[1024];
    int log_len;
    size_t total_len;
    va_list args;

    va_start(args, format);
    log_len = vsnprintf(output_buf, sizeof(output_buf) - 3U, format, args);
    va_end(args);

    if (log_len < 0)
    {
        return;
    }

    total_len = (size_t)log_len;
    if (total_len > sizeof(output_buf) - 3U)
    {
        total_len = sizeof(output_buf) - 3U;
    }

    output_buf[total_len++] = '\r';
    output_buf[total_len++] = '\n';
    output_buf[total_len] = '\0';

    elog_port_output(output_buf, total_len);
}
