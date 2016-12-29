# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.

# MediaTek Inc. (C) 2010. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
# THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
# CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
# SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
# CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
#
# The following software/firmware and/or related documentation ("MediaTek Software")
# have been modified by MediaTek Inc. All revisions are subject to any receiver's
# applicable license agreements with MediaTek Inc.
ifeq ($(BOARD_HAVE_MTK_MT6620),true)
LOCAL_PATH:= $(call my-dir)

#
# libmtkplayer
#

ifneq ($(strip $(BOARD_USES_GENERIC_AUDIO)),true)

ifneq ($(strip $(HAVE_MATV_FEATURE))_$(strip $(BOARD_USES_YUSU_AUDIO)), no_false)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    FMAudioPlayer.cpp

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl -lpthread
endif

LOCAL_SHARED_LIBRARIES :=     \
	libcutils             \
	libutils              \
	libbinder             \
	libmedia              \
	libandroid_runtime

#LOCAL_SHARED_LIBRARIES += libaudio.primary.default

## this MTK_MATV_SUPPORT define will be referenced by mATVAudioPlayer.cpp
ifeq ($(HAVE_MATV_FEATURE),yes)
  LOCAL_CFLAGS += -DMTK_MATV_SUPPORT
endif

ifeq ($(strip $(BOARD_USES_YUSU_AUDIO)),true)
  LOCAL_CFLAGS += -DMTK_AUDIO
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

ifeq ($(strip $(BOARD_USES_GENERIC_AUDIO)),true)
  LOCAL_CFLAGS += -DFAKE_MATV
endif

ifeq ($(BOARD_HAVE_MTK_MT6620),true)
LOCAL_CFLAGS += -DMTK_FM_SUPPORT
endif

#This is for customization
#LOCAL_SHARED_LIBRARIES += libaudiosetting

#use for Audio_ecutom_exp.h setting

ifeq ($(strip $(BOARD_USES_YUSU_AUDIO)),true)
  LOCAL_CFLAGS += -DMTK_AUDIO
endif
ifeq ($(strip $(BOARD_USES_YUSU_AUDIO)),true)
LOCAL_C_INCLUDE += $(MTK_PATH_CUSTOM)/hal/audioflinger/
endif


LOCAL_C_INCLUDES :=  \
	$(JNI_H_INCLUDE)                                                \
	$(TOP)/frameworks/base/services/audioflinger                    \
	$(TOP)/frameworks/base/include/media                            \
    $(TOP)/frameworks/base/include                                  \
	$(TOP)/$(MTK_PATH_SOURCE)/frameworks/base/include               \
	$(TOP)/$(MTK_PATH_SOURCE)/frameworks/base/include/media         \
	$(TOP)/external                                                 \
	$(TOP)/$(MTK_PATH_SOURCE)/external/AudioCompensationFilter \
	$(TOP)/$(MTK_PATH_SOURCE)/external/audiodcremoveflt

ifeq ($(strip $(MTK_PLATFORM)),MT6573)
  LOCAL_C_INCLUDES+= \
   $(TOP)/$(MTK_PATH_PLATFORM)/hardware/audio/aud_drv \
   $(TOP)/$(MTK_PATH_PLATFORM)/hardware/audio/LAD \
   $(TOP)/$(MTK_PATH_PLATFORM)/hardware/audio
endif
ifeq ($(strip $(MTK_PLATFORM)),MT6575)
  LOCAL_C_INCLUDES+= \
   $(TOP)/$(MTK_PATH_PLATFORM)/hardware/audio/aud_drv \
   $(TOP)/$(MTK_PATH_PLATFORM)/hardware/audio/LAD \
   $(TOP)/$(MTK_PATH_PLATFORM)/hardware/audio
endif


LOCAL_MODULE:= libmtkplayer

LOCAL_PRELINK_MODULE := no

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
endif
endif
