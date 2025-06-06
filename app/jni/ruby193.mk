# Ruby 1.9.3 configuration
LOCAL_PATH := $(call my-dir)/ruby193
LOCAL_BUILD_PATH := $(call my-dir)/build-$(TARGET_ARCH_ABI)

include $(CLEAR_VARS)

LOCAL_MODULE := ruby193

# 直接使用Ruby 1.9.3源码目录中构建的库文件
LOCAL_SRC_FILES := $(LOCAL_BUILD_PATH)/lib/libruby193.so

# 直接使用Ruby 1.9.3源码目录中的头文件
LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

include $(PREBUILT_SHARED_LIBRARY)