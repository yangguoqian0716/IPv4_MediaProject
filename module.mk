LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := e2prom_bl24c512a
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_INSTALLED_KO_FILES := e2prom_bl24c512a.ko
include $(BUILD_DEVICE_KO)
