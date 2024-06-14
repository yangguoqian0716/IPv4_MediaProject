ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
 
obj-m += e2prom_bl24c512a.o
e2prom_bl24c512a-objs := eeprom_BL24C512A.o
e2prom_bl24c512a-objs += drv_whitelist.o
