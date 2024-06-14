/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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

#ifndef _DRV_WHITELIST_H
#define _DRV_WHITELIST_H

#include <linux/list.h>
#include "drv_log.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC                  static
#endif

/* error code */
#define WL_OK                   0
#define WL_ERR_PARA             (-1)
#define WL_ERR_SHA256_CHECK     (-2)
#define WL_ERR_MEM_ALLOC        (-3)
#define WL_ERR_MEM_CMP          (-4)
#define WL_ERR_MEM_CPY          (-5)
#define WL_ERR_NO_AUTHORITY		(-6)
#define WL_ERR_FILE_OPEN        (-7)
#define WL_ERR_FILE_READ        (-8)
#define WL_ERR_FILE_FORMAT      (-9)
#define WL_ERR_MEM_INIT         (-10)
#define WL_ERR_SOC_VERIFY      (-11)

/* log */
#define WL_LOG_PREFIX "whitelist"

#define WHITELIST_ERR(fmt, ...)                                                                 \
    do {                                                                                        \
        drv_err(WL_LOG_PREFIX, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__);    \
    } while (0);
#define WHITELIST_WARN(fmt, ...)                                                                 \
    do {                                                                                         \
        drv_warn(WL_LOG_PREFIX, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__);    \
    } while (0);
#define WHITELIST_INFO(fmt, ...)                                                                 \
    do {                                                                                         \
        drv_info(WL_LOG_PREFIX, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__);    \
    } while (0);

/* process white list */
#define PROCESS_EXCUTABLE_FLAG          0x75
#define WL_NAME_NUM_MAX                 50

#ifdef CFG_SOC_PLATFORM_CLOUD
#define SOC_HEAD_SIZE 0x10000
#define NVCNT_INFO_SIZE 8
#elif (defined CFG_SOC_PLATFORM_MINIV2) || (defined CFG_SOC_PLATFORM_MINIV2_MDC)
#define SOC_HEAD_SIZE 0x2000
#define NVCNT_INFO_SIZE 0
#else
#define SOC_HEAD_SIZE 0x4000
#define NVCNT_INFO_SIZE 0
#endif

/*
 * desc: Driver process whitelist check handler
 * in:   const char **list name   the process whitelist name pointer
 *       u32 name_num  whitelist name num
 * ret:  0: check success   others: failed
 */
s32 whitelist_process_handler(const char **list_name, u32 name_num);

#endif
