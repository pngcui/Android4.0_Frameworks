LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    AudioFlinger.cpp            \
    AudioMixer.cpp.arm          \
    AudioResampler.cpp.arm      \
    AudioResamplerSinc.cpp.arm  \
    AudioResamplerCubic.cpp.arm \
    AudioPolicyService.cpp

LOCAL_C_INCLUDES := \
    system/media/audio_effects/include

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libbinder \
    libmedia \
    libhardware \
    libhardware_legacy \
    libeffects \
    libdl \
    libpowermanager

LOCAL_STATIC_LIBRARIES := \
    libcpustats \
    libmedia_helper

LOCAL_MODULE:= libaudioflinger

ifeq ($(BOARD_HAVE_MTK_MT6620),true)
LOCAL_CFLAGS += -DMTK_FM_SUPPORT
endif

ifeq ($(USE_ULP_AUDIO),true)
LOCAL_CFLAGS += -DSLSI_ULP_AUDIO
LOCAL_C_INCLUDES += device/samsung/sec_mm/sec_codecs/s5pc210/audio/srp/include
LOCAL_STATIC_LIBRARIES += libsrpapi
endif

include $(BUILD_SHARED_LIBRARY)
