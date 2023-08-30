LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := m8c

LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/src/*.c)

LOCAL_EXPORT_C_INCLUDES := $(wildcard $(LOCAL_PATH)/src/*.h)

LOCAL_CFLAGS += -DUSE_LIBUSB

LOCAL_SHARED_LIBRARIES := usb-1.0 SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
