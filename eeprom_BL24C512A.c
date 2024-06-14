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
#ifndef DRV_UT
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/types.h>
#include "linux/securec.h"
#include "eeprom_BL24C512A.h"
#include "drv_whitelist.h"

dev_t devno;
static struct class *cls = NULL;
static struct device *node_device[EEPROM_I2C_MINORS] = {NULL};
struct cdev node_cdev_arr[EEPROM_I2C_MINORS] = {0};
struct mutex i2c_dev_array_lock;
static struct bl24c512a_dev *i2c_dev_array[EEPROM_I2C_MINORS] = {NULL};
 
/*
static const struct i2c_device_id bl24c512a_idtable[] = {
    { "eeprom_bl24c512a", 0 },
    { "eeprom_bl24c512a_mini", 0 },
    { }
};
*/

static const struct i2c_device_id bl24c512a_idtable[] = {
    { "eeprom_bl24c512a", 0 },
    { }
};

/*lint -e508 */
MODULE_DEVICE_TABLE(i2c, bl24c512a_idtable); /*lint +e508 */

STATIC struct bl24c512a_dev *get_free_i2c_dev(struct i2c_adapter *adap)
{
    struct bl24c512a_dev *i2c_dev = NULL;
    int device_count = 0;

    if (adap == NULL) {
        eeprom_err("invalid parameter,adap == NULL\n");
        return NULL;
    }

    if (adap->nr >= EEPROM_I2C_MINORS) {
        eeprom_err("invalid parameter,adap->nr = %d\n", adap->nr);
        return NULL;
    }

    i2c_dev = kzalloc(sizeof(struct bl24c512a_dev), GFP_KERNEL);
    if (i2c_dev == NULL) {
        eeprom_err("kzalloc i2c_dev error \n");
        return NULL;
    }

    mutex_lock(&i2c_dev_array_lock);
    for (device_count = 0; device_count < EEPROM_I2C_MINORS; device_count++) {
        if (i2c_dev_array[device_count] == NULL) {
            i2c_dev_array[device_count] = i2c_dev;
            break;
        }
    }
    mutex_unlock(&i2c_dev_array_lock);

    if (device_count >= EEPROM_I2C_MINORS) {
        eeprom_err("error: eeprom device num is more than %d\n", EEPROM_I2C_MINORS);
        goto exit;
    }

    eeprom_info("eeprom id:%d\n", device_count);
    i2c_dev->minor = device_count;
    mutex_init(&(i2c_dev->mutex));

    return i2c_dev;
exit:
    kfree(i2c_dev);
    i2c_dev = NULL;
    return NULL;
}

STATIC int return_i2c_dev(struct bl24c512a_dev *i2c_dev)
{
    if (i2c_dev == NULL) {
        eeprom_err("invalid parameter,i2c_dev == NULL\n");
        return -1;
    }

    if (i2c_dev->minor >= EEPROM_I2C_MINORS) {
        eeprom_err("invalid parameter,i2c_dev->minor = %d\n", i2c_dev->minor);
        return -1;
    }
    mutex_lock(&i2c_dev_array_lock);
    i2c_dev_array[i2c_dev->minor] = NULL;
    mutex_unlock(&i2c_dev_array_lock);
    (void)mutex_destroy(&(i2c_dev->mutex));
    kfree(i2c_dev);
    i2c_dev = NULL;

    return 0;
}

STATIC struct bl24c512a_dev *i2c_dev_get_by_minor(unsigned index)
{
    struct bl24c512a_dev *i2c_dev = NULL;

    if (index >= EEPROM_I2C_MINORS) {
        eeprom_err("invalid parameter,index = %d\n", index);
        return NULL;
    }

    mutex_lock(&i2c_dev_array_lock);
    i2c_dev = i2c_dev_array[index];
    mutex_unlock(&i2c_dev_array_lock);

    return i2c_dev;
}

STATIC int eeprom_open(struct inode *node, struct file *file)
{
    unsigned int minor;
    struct bl24c512a_dev *i2c_dev = NULL;

    eeprom_info("eeprom open\n");

    if (node == NULL || file == NULL) {
        eeprom_err("eeprom open input arg is NULL\n");
        return -ENODEV;
    }
    minor = iminor(node);
    eeprom_info("minor:%d\n", minor);
    i2c_dev = i2c_dev_get_by_minor(minor);
    if (i2c_dev == NULL) {
        eeprom_err("i2c_dev is NULL, eeprom_open fail!\n");
        return -ENODEV;
    }

    file->private_data = i2c_dev;
    return 0;
}

STATIC int eeprom_release(struct inode *node, struct file *file)
{
    struct bl24c512a_dev *i2c_dev = NULL;

    if (node == NULL || file == NULL) {
        eeprom_err("eeprom release input arg is NULL\n");
        return -ENODEV;
    }

    i2c_dev = (struct bl24c512a_dev *)file->private_data;
    if (i2c_dev == NULL) {
        eeprom_err("invalid parameter,i2c_dev == NULL\n");
        return -1;
    }
    file->private_data = NULL;
    eeprom_info("eeprom release\n");

    return 0;
}

STATIC int eeprom_read(struct file *filp, unsigned char *buf, unsigned int count,
    unsigned short page_address, bool in_kernel)
{
    struct bl24c512a_dev *i2c_dev = (struct bl24c512a_dev *)filp->private_data;
    struct i2c_client *client = i2c_dev->client;
    int ret = -1;
    char message[EEPROM_READ_MESSAGE_LEN] = {0};
    char *read_buf = NULL;
    char *read_buf_temp = NULL;
    unsigned int read_loop;
    unsigned int read_mod;
    unsigned int i = 0;

    if (page_address + count > EEPROM_MAX_ADDR) {
        eeprom_err("eeprom read address range beyond or count invalid!\n");
        page_address = 0;
        return -1;
    }

    if (count <= 0) {
        eeprom_err("eeprom read address count invalid!\n");
        return -1;
    }

    read_buf = (char *)kzalloc(count, GFP_KERNEL);
    if (read_buf == NULL) {
        eeprom_err("eeprom_read: kmalloc error!\n");
        return -ENOMEM;
    }
    read_loop = (unsigned int)(count / EEPROM_READ_BLOCK);
    read_mod = (unsigned int)(count % EEPROM_READ_BLOCK);

    for (i = 0; i < read_loop; i++) {
        mdelay(EEPROM_DELAY_MS);
        message[0] = (u8)(page_address >> EEPROM_PAGE_ADDR_BITS);
        message[1] = (u8)(page_address);
        ret = i2c_master_send(client, message, EEPROM_READ_MESSAGE_LEN);
        if (ret != 2) {
            eeprom_err("eeprom_read master_send error! ret = %d\n", ret);
            ret = 0;
            goto out;
        }

        read_buf_temp = &read_buf[(long)i * EEPROM_READ_BLOCK];
        ret = i2c_master_recv(client, read_buf_temp, EEPROM_READ_BLOCK);
        if (ret != EEPROM_READ_BLOCK) {
            eeprom_err("EEPROM READ recv fail! ret = %d\n", ret);
            ret = 0;
            goto out;
        }
        page_address = page_address + (unsigned int)EEPROM_READ_BLOCK;
    }

    if (read_mod != 0) {
        mdelay(EEPROM_DELAY_MS);
        message[0] = (u8)(page_address >> EEPROM_PAGE_ADDR_BITS);
        message[1] = (u8)(page_address);
        ret = i2c_master_send(client, message, EEPROM_READ_MESSAGE_LEN);
        if (ret != 2) {
            eeprom_err("eeprom_read master_send error! ret = %d\n", ret);
            ret = 0;
            goto out;
        }

        read_buf_temp = &read_buf[(long)i * EEPROM_READ_BLOCK];
        ret = i2c_master_recv(client, read_buf_temp, read_mod);
        if (read_mod != ret) {
            eeprom_err("EEPROM READ recv fail! ret = %d\n", ret);
            ret = 0;
            goto out;
        }
    }

    eeprom_info("EEPROM READ finish!\n");
    if (in_kernel) {
        if (memcpy_s((void *)buf, count, (const void *)read_buf, count)) {
            eeprom_err("copy failed\n");
            ret = -EFAULT;
            goto out;
        }
    } else {
        if ((buf == NULL) || copy_to_user(buf, (void *)read_buf, count)) {
            eeprom_err("copy_to_user failed\n");
            ret = -EFAULT;
            goto out;
        }
    }

    ret = count;
    goto out;

out:
    kfree(read_buf);
    read_buf = NULL;
    return ret;
}

STATIC int eeprom_write_para_check(char **message, const unsigned char *buf,
    unsigned int count, unsigned short page_address)
{
    unsigned int buff_alloc_size;
    if (page_address + count > EEPROM_MAX_ADDR) {
        eeprom_err("eeprom write address range beyond\n");
        return -1;
    }

    buff_alloc_size = count + EEPROM_WRITE_ADDR_LEN;
    if (buff_alloc_size <= EEPROM_WRITE_ADDR_LEN || buff_alloc_size > EEPROM_MAX_ADDR + EEPROM_WRITE_ADDR_LEN) {
        eeprom_err("eeprom write buff_alloc_size range beyond\n");
        return -1;
    }
    *message = (char *)kzalloc(buff_alloc_size, GFP_KERNEL);
    if (*message == NULL) {
        eeprom_err("eeprom_write: kmalloc error!\n");
        return -ENOMEM;
    }
    if ((buf == NULL) || (copy_from_user(&(*message)[EEPROM_WRITE_ADDR_LEN], (void *)buf, count) != 0)) {
        eeprom_err("eeprom write copy_from_user failed\n");
        kfree(*message);
        *message = NULL;
        return -1;
    }
    return 0;
}

STATIC int eeprom_write(struct file *file, const unsigned char *buf, unsigned int count, unsigned short page_address)
{
    struct bl24c512a_dev *i2c_dev = (struct bl24c512a_dev *)file->private_data;
    struct i2c_client *client = i2c_dev->client;
    const char *wl_process_name = "dmp_daemon";
    unsigned int write_loop;
    unsigned int write_mod;
    int ret = -1;
    unsigned int i = 0;
    char *message = NULL;
    char *message_temp = NULL;
    unsigned short page_address_align_up;
    unsigned int remain_count = 0;

    ret = whitelist_process_handler(&wl_process_name, 1);
    if (ret) {
        eeprom_err("eeprom write permission denied!\n");
        return -EPERM;
    }

    ret = eeprom_write_para_check(&message, buf, count, page_address);
    if (ret) {
        return ret;
    }

    page_address_align_up = ALIGN_UP(page_address, EEPROM_WRITE_BLOCK);
    if ((page_address % EEPROM_BANK_SIZE) != 0 && (page_address + count > page_address_align_up)) {
        message[0] = (u8)(page_address >> EEPROM_PAGE_ADDR_BITS);
        message[1] = (u8)(page_address);

        ret = i2c_master_send(client, message, page_address_align_up - page_address + 2);
        if (page_address_align_up - page_address + EEPROM_WRITE_ADDR_LEN != ret) {
            eeprom_err("EEPROM WRITE send fail! ret = %d\n", ret);
            ret = 0;
            goto out;
        }
        remain_count = count - (page_address_align_up - page_address);
        message_temp = &message[page_address_align_up - page_address];
        page_address = page_address_align_up;
    } else {
        remain_count = count;
        message_temp = message;
    }

    write_loop = (unsigned int)(remain_count / EEPROM_WRITE_BLOCK);
    write_mod = (unsigned int)(remain_count % EEPROM_WRITE_BLOCK);

    for (i = 0; i < write_loop; i++) {
        mdelay(EEPROM_DELAY_MS);
        message_temp[0 + ((long)i * EEPROM_WRITE_BLOCK)] = (u8)(page_address >> EEPROM_PAGE_ADDR_BITS);
        message_temp[1 + ((long)i * EEPROM_WRITE_BLOCK)] = (u8)(page_address);

        ret = i2c_master_send(client, &message_temp[(long)i * EEPROM_WRITE_BLOCK],
            EEPROM_WRITE_BLOCK + EEPROM_WRITE_ADDR_LEN);
        if (ret != EEPROM_WRITE_BLOCK + EEPROM_WRITE_ADDR_LEN) {
            eeprom_err("EEPROM WRITE send fail! ret = %d\n", ret);
            ret = 0;
            goto out;
        }
        page_address = page_address + (unsigned int)EEPROM_WRITE_BLOCK;
    }

    if (write_mod != 0) {
        mdelay(EEPROM_DELAY_MS);
        message_temp[0 + ((long)i * EEPROM_WRITE_BLOCK)] = (u8)(page_address >> EEPROM_PAGE_ADDR_BITS);
        message_temp[1 + ((long)i * EEPROM_WRITE_BLOCK)] = (u8)(page_address);
        ret = i2c_master_send(client, &message_temp[(long)i * EEPROM_WRITE_BLOCK], write_mod + EEPROM_WRITE_ADDR_LEN);
        if (write_mod + EEPROM_WRITE_ADDR_LEN != ret) {
            eeprom_err("EEPROM WRITE send fail! ret = %d\n", ret);
            ret = 0;
            goto out;
        }
    }
    eeprom_info("EEPROM WRITE finish!\n");
    ret = count;
    goto out;

out:
    kfree(message);
    message = NULL;
    return ret;
}

long eeprom_ioctl_process(struct file *file,
    unsigned int cmd, struct eeprom_info *IoctlMsg, bool in_kernel)
{
    struct bl24c512a_dev *i2c_dev = NULL;
    int ret = 0;
    unsigned char *buff_addr = NULL;

    if (file == NULL) {
        eeprom_err("invalid parameter, file = NULL\n");
        return -1;
    }

    i2c_dev = (struct bl24c512a_dev *)file->private_data;
    if (i2c_dev == NULL) {
        eeprom_err("invalid parameter,i2c_dev = NULL\n");
        return -1;
    }

    if (IoctlMsg->page_address > EEPROM_MAX_ADDR) {
        eeprom_err("eeprom address range beyond\n");
        return -1;
    }

    if (IoctlMsg->buf == NULL) {
        eeprom_err("IoctlMsg buf is NULL!\n");
        return -1;
    }

    if (IoctlMsg->count <= 0 || IoctlMsg->count > EEPROM_MAX_ADDR) {
        eeprom_err("IoctlMsg count error!\n");
        return -1;
    }

    buff_addr = (unsigned char *)IoctlMsg->buf;
    switch (cmd) {
        case EEPROM_WRITE_CMD:
            mutex_lock(&(i2c_dev->mutex));
            ret = eeprom_write(file, (const unsigned char *)buff_addr, IoctlMsg->count,
                (unsigned short)IoctlMsg->page_address);
            mutex_unlock(&(i2c_dev->mutex));
            break;
        case EEPROM_READ_CMD:
            mutex_lock(&(i2c_dev->mutex));
            ret = eeprom_read(file, buff_addr, IoctlMsg->count,
                (unsigned short)IoctlMsg->page_address, in_kernel);
            mutex_unlock(&(i2c_dev->mutex));
            break;
        default:
            eeprom_err("eeprom cmd error!\n");
            return -1;
    }

    return ret;
}

long eeprom_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct eeprom_info IoctlMsg;
    long ret = 0;
    void *arg_temp = (void *)(uintptr_t)arg;

    if (arg_temp == NULL) {
        eeprom_err("invalid parameter, arg_temp = NULL\n");
        return -1;
    }

    if (copy_from_user(&IoctlMsg, arg_temp, sizeof(struct eeprom_info))) {
        eeprom_err("copy_from_user fail!\n");
        return -1;
    }

    ret = eeprom_ioctl_process(file, cmd, &IoctlMsg, false);

    return ret;
}

long eeprom_ioctl_kernel(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    struct eeprom_info *IoctlMsg = (struct eeprom_info *)(uintptr_t)arg;
    if (IoctlMsg == NULL) {
        eeprom_err("invalid parameter, arg_temp = NULL\n");
        return -1;
    }

    ret = eeprom_ioctl_process(file, cmd, IoctlMsg, true);

    return ret;
}
EXPORT_SYMBOL(eeprom_ioctl_kernel);

STATIC struct file_operations eeprom_fops = {
    .owner = THIS_MODULE,
    .open = eeprom_open,
    .release = eeprom_release,
    .unlocked_ioctl = eeprom_ioctl,
};

STATIC int eeprom_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = NULL;
    struct bl24c512a_dev *i2c_dev = NULL;
    unsigned char __iomem *base_addr = NULL;
    int ret;

    eeprom_info("eeprom_probe enter!\n");
    /* rest of the initialisation goes here. */
    eeprom_info("eeprom client created\n");
    adapter = client->adapter;
    i2c_dev = get_free_i2c_dev(adapter);
    if (i2c_dev == NULL) {
        eeprom_err("eeprom_probe: i2c_dev is NULL!\n");
        return -1;
    }

    eeprom_info("eeprom adapter [%s] registered as minor %d\n", adapter->name, i2c_dev->minor);

    i2c_dev->adap = adapter;
    i2c_dev->client = client;

    if (!(base_addr = ioremap(IOMUX_REG_ADDR, IOMUX_REG_LEN))) {
        eeprom_err("eeprom ioremap fail!\n");
        return -EFAULT;
    } else {
        writel(IOMUX_I2C_VALUE, (volatile unsigned char __iomem *)((long)(uintptr_t)base_addr + IOMUX_REG_10_OFFSET));
        writel(IOMUX_I2C_VALUE, (volatile unsigned char __iomem *)((long)(uintptr_t)base_addr + IOMUX_REG_14_OFFSET));
        iounmap(base_addr);
        base_addr = NULL;
    }

    if (i2c_dev->minor == 0) {
        node_device[i2c_dev->minor] = device_create(cls, NULL, devno, NULL, EEPROM_NAME);
    } else {
        node_device[i2c_dev->minor] =
            device_create(cls, NULL, devno + i2c_dev->minor, NULL, "eeprom_bl24c512a-%d", i2c_dev->minor);
    }

    if (IS_ERR(node_device[i2c_dev->minor])) {
        eeprom_err("device create error!\n");
        return -EBUSY;
    }

    cdev_init(&(node_cdev_arr[i2c_dev->minor]), &eeprom_fops);
    node_cdev_arr[i2c_dev->minor].owner = THIS_MODULE;
    ret = cdev_add(&(node_cdev_arr[i2c_dev->minor]), devno + i2c_dev->minor, EEPROM_I2C_MINORS);
    if (ret) {
        device_destroy(cls, devno + i2c_dev->minor);
        eeprom_err("Error %d adding node_cdev!\n", ret);
        return ret;
    }

    return 0;
}

STATIC int eeprom_remove(struct i2c_client *client)
{
    if (client == NULL) {
        eeprom_err("invalid parameter,client == NULL\n");
        return -1;
    }
    return 0;
}

static struct i2c_driver eeprom_driver = {
    .driver = {
        .owner		= THIS_MODULE,
        .name	   = "eeprom_bl24c512a",
    },
    .id_table			 = bl24c512a_idtable,
    .probe		= eeprom_probe,
    .remove		= eeprom_remove,
};

STATIC int __init eeprom_init(void)
{
    int ret = -1;

    mutex_init(&i2c_dev_array_lock);
    ret = memset_s(i2c_dev_array, sizeof(i2c_dev_array), 0, sizeof(i2c_dev_array));
    if (ret != 0) {
        eeprom_err("memset_s error!\n");
        return -1;
    }

    ret = alloc_chrdev_region(&devno, 0, EEPROM_I2C_MINORS, EEPROM_NAME);
    if (ret < 0) {
        eeprom_err("%s: alloc_chrdev_region fail! ret = %d\n", EEPROM_NAME, ret);
        return ret;
    }

    cls = class_create(THIS_MODULE, EEPROM_NAME);
    if (IS_ERR(cls)) {
        ret = PTR_ERR(cls);
        unregister_chrdev_region(devno, EEPROM_I2C_MINORS);
        eeprom_err("class create error %d\n", ret);
        return -EBUSY;
    }

    ret = i2c_add_driver(&eeprom_driver);
    if (ret != 0) {
        eeprom_err("i2c add eeprom_driver error!\n");
        unregister_chrdev_region(devno, EEPROM_I2C_MINORS);
        class_destroy(cls);
        return ret;
    }

    eeprom_info("i2c_add_driver finish!\n");
    return 0;
}

STATIC void __exit eeprom_exit(void)
{
    int device_count;
    struct bl24c512a_dev *i2c_dev = NULL;

    for (device_count = 0; device_count < EEPROM_I2C_MINORS; device_count++) {
        if (NULL != i2c_dev_array[device_count]) {
            i2c_dev = i2c_dev_array[device_count];
            return_i2c_dev(i2c_dev);
            cdev_del(&node_cdev_arr[device_count]);
            device_destroy(cls, devno + device_count);
        }
    }

    unregister_chrdev_region(devno, EEPROM_I2C_MINORS);
    class_destroy(cls);
    i2c_del_driver(&eeprom_driver);
    (void)mutex_destroy(&i2c_dev_array_lock);
    eeprom_info("eeprom_exit finish!\n");
}

MODULE_DESCRIPTION("eeprom driver");
MODULE_LICENSE("GPL");

module_init(eeprom_init);
module_exit(eeprom_exit);
#else
int eeprom_write(void)
{
    return 0;
}
#endif
