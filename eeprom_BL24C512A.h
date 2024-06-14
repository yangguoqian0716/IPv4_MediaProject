/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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
#ifndef __EEPROM_BL24C512A_H__
#define __EEPROM_BL24C512A_H__

#include <linux/mutex.h>
#include "drv_log.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define EEPROM_LOG_PREFIX "eeprom"
#define eeprom_err(fmt...) drv_err(EEPROM_LOG_PREFIX, fmt)
#define eeprom_warn(fmt...) drv_warn(EEPROM_LOG_PREFIX, fmt)
#define eeprom_info(fmt...) drv_info(EEPROM_LOG_PREFIX, fmt)
#define eeprom_debug(fmt...) drv_debug(EEPROM_LOG_PREFIX, fmt)

#define EEPROM_IOC_MAGIC 'E'
#define EEPROM_WRITE_CMD _IOR(EEPROM_IOC_MAGIC, 42, int)
#define EEPROM_READ_CMD _IOR(EEPROM_IOC_MAGIC, 43, int)

#define EEPROM_I2C_MINORS 10
/*
#define EEPROM_NAME "eeprom_m24256"
*/
#define EEPROM_NAME "eeprom_bl24c512a"
#define I2C_EEPROM_ADDR (0x57 << 1) // 0x54
#define I2C_EEPROM_ID 777
#define EEPROM_BANK_SIZE 64
#define I2C_NAME(x) (x)->name
/* 256Kbit equal to 32Kbyte
#define EEPROM_MAX_ADDR 0x7fff
*/
#define EEPROM_MAX_ADDR 0xffff /* 512Kbit equal to 64Kbyte */
#define EEPROM_READ_BLOCK 128
#define EEPROM_WRITE_BLOCK 64
#define ALIGN_UP(value, base) (((value) + ((base) - 1)) & ~((base) - 1))

#define IOMUX_REG_ADDR (0x0110080000)
#define IOMUX_REG_LEN (0x18)
#define IOMUX_REG_10_OFFSET (0x10)
#define IOMUX_REG_14_OFFSET (0x14)
#define IOMUX_I2C_VALUE (0x02)
#define EEPROM_READ_MESSAGE_LEN (0x2)
#define EEPROM_WRITE_ADDR_LEN (0x2)
#define EEPROM_PAGE_ADDR_BITS (8)
#define EEPROM_DELAY_MS (10)

struct eeprom_state {
    struct i2c_client *client;
    struct input_dev *input_dev;
};


struct bl24c512a_dev {
    int minor;
    struct i2c_adapter *adap;
    struct i2c_client *client;
    struct mutex mutex;
};

/*
struct m24256_dev {
    int minor;
    struct i2c_adapter *adap;
    struct i2c_client *client;
    struct mutex mutex;
};
*/
struct eeprom_info {
    unsigned char *buf;
    unsigned int count;
    unsigned int page_address;
};


#ifdef STATIC_SKIP
STATIC void __exit eeprom_exit(void);
STATIC int __init eeprom_init(void);
STATIC int eeprom_remove(struct i2c_client *client);
STATIC int eeprom_probe(struct i2c_client *client, const struct i2c_device_id *id);
STATIC int eeprom_write(struct file *file, const unsigned char *buf_addr, unsigned int count,
    unsigned short page_address);
STATIC int eeprom_read(struct file *filp, unsigned char *buf_addr, unsigned int count,
    unsigned short page_address, bool in_kernel);
STATIC int eeprom_release(struct inode *node, struct file *file);
STATIC int eeprom_open(struct inode *node, struct file *file);
STATIC struct bl24c512a_dev *i2c_dev_get_by_minor(unsigned index);
STATIC struct bl24c512a_dev *i2c_dev_get_by_adapter(struct i2c_adapter *adap);
STATIC int return_i2c_dev(struct bl24c512a_dev *i2c_dev);
STATIC struct bl24c512a_dev *get_free_i2c_dev(struct i2c_adapter *adap);
#endif

long eeprom_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
long eeprom_ioctl_kernel(struct file *file, unsigned int cmd, unsigned long arg);

#endif
