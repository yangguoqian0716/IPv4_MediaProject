/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */

#ifndef _DRV_LOG_H_
#define _DRV_LOG_H_

#define LOG_LEVEL_INFO_INPUT_LEN                2
#define LOG_LEVEL_FILE_INFO_LEN                 3
#define DEVDRV_HOST_LOG_FILE_CREAT_AUTHORITY    0664
int log_level_file_init(void);
void log_level_file_remove(void);
int log_level_get(void);

/* host ko depmod */
#define PCI_DEVICE_CLOUD 0xa126
#define LOG_BUF_SIZE_MAX 100

typedef struct log_buf_info {
    unsigned int log_size;
    char log_buf[LOG_BUF_SIZE_MAX];
} log_buf_info_t;

#define drv_printk(level, module, fmt, ...) \
    (void)printk(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#if (defined(LOG_UT) || defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11))
#define drv_err(module, fmt...) drv_printk(KERN_ERR, module, fmt)
#else
#define logflow_printk(level, module, fmt, ...) \
    (void)printk(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
#define drv_err(module, fmt...) logflow_printk(KERN_ERR, module, fmt)
#endif

#define drv_warn(module, fmt, ...) drv_printk(KERN_WARNING, module, fmt, ##__VA_ARGS__)
#define drv_info(module, fmt, ...) drv_printk(KERN_INFO, module, fmt, ##__VA_ARGS__)
#define drv_debug(module, fmt, ...) drv_printk(KERN_DEBUG, module, fmt, ##__VA_ARGS__)
#define drv_event(module, fmt, ...) drv_printk(KERN_NOTICE, module, fmt, ##__VA_ARGS__)
#define drv_pr_debug(module, fmt, ...) \
    pr_debug("[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define drv_err_spinlock(module, fmt, ...) drv_err(module, fmt, ##__VA_ARGS__)
#define drv_warn_spinlock(module, fmt, ...) drv_warn(module, fmt, ##__VA_ARGS__)
#define drv_info_spinlock(module, fmt, ...) drv_info(module, fmt, ##__VA_ARGS__)
#define drv_debug_spinlock(module, fmt, ...) drv_debug(module, fmt, ##__VA_ARGS__)
#define drv_event_spinlock(module, fmt, ...) drv_event(module, fmt, ##__VA_ARGS__)

/**
 * Description of log interfaces used in special scenarios
 * for example: record logs in the spin_lock
 * drv_log_init() // only can be called once within a function
 * spin_lock_xxx
 * drv_err_log_save() // can be called multiple times within a function
 * spin_unlock_xxx
 * drv_log_output() // can be called multiple times within a function
 *
 * Precautions:
 * (1) drv_log_init(), drv_err_log_save() and drv_log_output() must be used together in the same function;
 * (2) drv_log_init() must be called at the variable definition, otherwise there will be a compilation error;
 * (3) the max size of logs can be saved continuously is LOG_BUF_SIZE_MAX bytes;
 * (4) only error logs can be recorded.
 */
#define drv_log_init() \
    log_buf_info_t buf_info = {0}
#define DRV_LOG_SAVE(level_code, module, fmt, ...) do { \
    int ret; \
    ret = snprintf_s(buf_info.log_buf + buf_info.log_size, LOG_BUF_SIZE_MAX - buf_info.log_size, \
        LOG_BUF_SIZE_MAX - buf_info.log_size - 1, \
        level_code "[ascend] [ERROR] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__); \
    if (ret > 0) { \
        buf_info.log_size += ret; \
    } \
} while (0)
#define drv_err_log_save(module, fmt, ...) do { \
    if (buf_info.log_size < (LOG_BUF_SIZE_MAX - 1)) { \
        if (buf_info.log_size == 0) { \
            DRV_LOG_SAVE(KERN_NOTICE, module, fmt, ##__VA_ARGS__); \
        } else { \
            DRV_LOG_SAVE("", module, fmt, ##__VA_ARGS__); \
        } \
    } \
} while (0)
#define drv_log_output() do { \
    if (buf_info.log_size > 0) { \
        (void)printk(buf_info.log_buf); \
        buf_info.log_size = 0; \
    } \
} while (0)
#endif /* _DRV_LOG_H_ */
