LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := drv_lm75
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_INSTALLED_KO_FILES := drv_lm75.ko
include $(BUILD_DEVICE_KO)