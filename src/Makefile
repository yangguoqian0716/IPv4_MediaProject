# KERNELDIR := "/opt/Ascend310B-source/driver/kernel/linux-4.19"
# CURRENT_PATH := $(shell pwd)
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
 
# obj-m += myhello.o
# myhello-objs := hello.o

obj-m += drv_lm75.o
drv_lm75-objs := lm75.o

# all:
#         $(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
# clean:
#         $(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean