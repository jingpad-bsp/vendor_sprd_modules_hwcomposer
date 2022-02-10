LOCAL_PATH:= $(call my-dir)

ifeq ($(strip $(SPRD_TARGET_USES_HWC2)),true)
include $(LOCAL_PATH)/v2.x/Android.mk
else
include $(LOCAL_PATH)/v1.x/Android.mk
endif
