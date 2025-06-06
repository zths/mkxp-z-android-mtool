L_PATH := $(call my-dir)

# Include dependencies
include $(L_PATH)/SDL2/Android.mk 
include $(L_PATH)/SDL2_image/Android.mk
include $(L_PATH)/SDL2_ttf/Android.mk
include $(L_PATH)/SDL2_sound.mk
include $(L_PATH)/libogg.mk
include $(L_PATH)/libvorbis.mk
include $(L_PATH)/libtheora.mk
include $(L_PATH)/openal.mk
include $(L_PATH)/pixman.mk
include $(L_PATH)/physfs.mk
include $(L_PATH)/uchardet.mk
include $(L_PATH)/libiconv.mk
include $(L_PATH)/openssl.mk

include $(L_PATH)/ruby.mk
include $(L_PATH)/mkxp-z.mk

include $(L_PATH)/ruby187.mk
include $(L_PATH)/mkxp-z-187.mk

include $(L_PATH)/ruby193.mk
include $(L_PATH)/mkxp-z-193.mk