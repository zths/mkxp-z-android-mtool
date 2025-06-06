# Ruby 1.8.7 configuration
LOCAL_PATH := $(call my-dir)/ruby187
LOCAL_BUILD_PATH := $(call my-dir)/build-$(TARGET_ARCH_ABI)

include $(CLEAR_VARS)

LOCAL_MODULE := ruby187

# 直接使用Ruby 1.8.7源码目录中构建的库文件
LOCAL_SRC_FILES := $(LOCAL_BUILD_PATH)/lib/libruby187.so

# 直接使用Ruby 1.8.7源码目录中的头文件
LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

include $(PREBUILT_SHARED_LIBRARY) 