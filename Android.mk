# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)


LOCAL_SRC_FILES := at_ppp.c
LOCAL_CFLAGS += -pie -fPIE
LOCAL_CFLAGS += -DUSE_NDK
LOCAL_LDFLAGS += -pie -fPIE
LOCAL_MODULE_TAGS:= optional


ifneq (foo,foo)
# to generate executable 
LOCAL_MODULE:= auto_find_ttyUSB
include $(BUILD_EXECUTABLE)
else
# shared libs
LOCAL_MODULE        :=  mytest
# LOCAL_C_INCLUDES    :=  $(LOCAL_PATH)/jni
include $(BUILD_SHARED_LIBRARY)
endif
