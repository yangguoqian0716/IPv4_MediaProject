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

#include <linux/ide.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include "lm75.h"

struct lm75_dev {
    dev_t devid; /* 设备号 */
    int major; /* 主设备号 */
    int minor; /* 次设备号 */
    struct cdev cdev; /* cdev */
    struct class *class; /* 类 */
    struct device *device; /* 设备 */
    struct device_node *nd; /* 设备节点 */
    void *private_data; /* 私有数据 */
};

static struct lm75_dev lm75_b_dev;

static int lm75_open(struct inode *inode, struct file *filep)
{
    printk("lm75 open\n");
    filep->private_data = &lm75_b_dev;
    return 0;
}

static int lm75_close(struct inode *inode, struct file *filep)
{
    printk("lm75 close\n");
	return 0;
}

static int lm75_read_regs(struct lm75_dev *dev, u8 reg, void *val, int len)
{
    int ret;
    struct i2c_msg msg[2];
    struct i2c_client *client;

    client = (struct i2c_client *)dev->private_data;
    msg[0].addr = (u16)0x49;
    msg[0].flags = 0;
    msg[0].buf = &reg;
    msg[0].len = 1;

    msg[1].addr = (u16)0x49; 
    msg[1].flags = I2C_M_RD;
    msg[1].buf = val;
    msg[1].len = len;
 
    ret = i2c_transfer(client->adapter, msg, 2);
    if(ret != 2) {
        printk("i2c read failed=%d reg=%02x len=%d\n",ret, reg, len);
        ret = -EREMOTEIO;
    }
    return 0;
}
static ssize_t lm75_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    int ret;
	unsigned char temp[LM75_READ_LEN];
	uint16_t temperature;
	int res;
    struct lm75_dev *dev;
    printk("lm75 read\n");

    dev = (struct lm75_dev *)filep->private_data;
    ret = lm75_read_regs(dev, LM75_REG_TEMP, temp, LM75_READ_LEN);
	if (ret != 0) {
        printk("lm75 read temp err(%d)!\n", ret);
		return ret;
	}
	printk("%02x-%02x\n", temp[0], temp[1]);
	temperature = (temp[0] << 8) | temp[1];		// 拼接两个字节的数据
    temperature >>= 5;							// lm75bd芯片手册解释需要后5位置舍弃，只需要11位
    if (temperature & 0x400) {                  // 如果11位为1，则温度为负数,负数特殊处理
        temperature = temperature | 0xf800U;
		temperature = ~temperature + 1;
		res = temperature * (-1);
    } else {
		res = temperature * 1;
	}
	
	if (copy_to_user((char *)buf, &res, sizeof(res))) {
        return -EFAULT;
    }
    return 0;
}

static const struct file_operations lm75_ops = {
	.owner = THIS_MODULE,
	.open = lm75_open,
	.release = lm75_close,
	.read = lm75_read,
};

static int xlq_lm75_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    printk("i2c probe");
    /* 1、构建设备号 */
    if (lm75_b_dev.major) {
        lm75_b_dev.devid = MKDEV(lm75_b_dev.major, 0);
        register_chrdev_region(lm75_b_dev.devid, 1, XLQ_LM75_NAME);
    } else {
        alloc_chrdev_region(&lm75_b_dev.devid, 0, 1, XLQ_LM75_NAME);
        lm75_b_dev.major = MAJOR(lm75_b_dev.devid);
    }

    /* 2、注册设备 */
    cdev_init(&lm75_b_dev.cdev, &lm75_ops);
    cdev_add(&lm75_b_dev.cdev, lm75_b_dev.devid, 1);

    /* 3、创建类 */
    lm75_b_dev.class = class_create(THIS_MODULE, XLQ_LM75_NAME);
    if (IS_ERR(lm75_b_dev.class)) {
        return PTR_ERR(lm75_b_dev.class);
    }

    /* 4、创建设备 */
    lm75_b_dev.device = device_create(lm75_b_dev.class, NULL,lm75_b_dev.devid, NULL, XLQ_LM75_NAME);
    if (IS_ERR(lm75_b_dev.device)) {
        return PTR_ERR(lm75_b_dev.device);
    }

    lm75_b_dev.private_data = client;
    printk("probe ok\n");
    return 0;
}

static int xlq_lm75_remove(struct i2c_client *client)
{
    printk("i2c remove");
    cdev_del(&lm75_b_dev.cdev);
    unregister_chrdev_region(lm75_b_dev.devid, 1);
    device_destroy(lm75_b_dev.class, lm75_b_dev.devid);
    class_destroy(lm75_b_dev.class);
    return 0;
}

static const struct i2c_device_id lm75_idtable[] = {
    {"lm75,lm75_b", 0}, 
    {}
};

/* 设备树匹配列表 */
static const struct of_device_id lm75_of_match[] = {
    { .compatible = "lm75,lm75_b" },
    {  }
};

static struct i2c_driver lm75_driver = {
    .probe		= xlq_lm75_probe,
    .remove		= xlq_lm75_remove,
    .driver = {
        .owner		= THIS_MODULE,
        .name	   = "lm75",
        .of_match_table = lm75_of_match,
    },
    .id_table			 = lm75_idtable,
};

static int __init lm75_init(void)
{
    int ret = -1;
    printk("lm75 init\n");
    ret = i2c_add_driver(&lm75_driver);
    if (ret != 0) {
        printk("i2c add driver error\n");
        return ret;
    }
    return 0;
}

static void __exit lm75_exit(void)
{
    printk("i2c del drivel\n");
    i2c_del_driver(&lm75_driver);
}

module_init(lm75_init);
module_exit(lm75_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("lm75");