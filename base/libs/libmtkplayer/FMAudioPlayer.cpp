/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

//#ifdef MTK_AUDIO


//#define LOG_NDEBUG 0
#define LOG_TAG "FMPlayer"
#include "utils/Log.h"

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <system/audio.h>

/*extern "C" {
#include "kal_release.h"
}*/

#include <binder/IServiceManager.h>

#include <AudioSystem.h>
#include "FMAudioPlayer.h"


//#define FM_AUDIO_FILELOG

#ifdef HAVE_GETTID
static pid_t myTid() { return gettid(); }
#else
static pid_t myTid() { return getpid(); }
#endif

static long long getTimeMs()
{
    struct timeval t1;
    long long ms;

    gettimeofday(&t1, NULL);
    ms = t1.tv_sec * 1000LL + t1.tv_usec / 1000;

    return ms;
}

// ----------------------------------------------------------------------------

namespace android {

// ----------------------------------------------------------------------------

//#define FAKE_FM
#define sineTable48KSIZE 480
#define sineTable32KSIZE 320
//sine table for simulation only
#ifdef FAKE_FM

static const uint16_t sineTable48K[480] = {
    0x1   ,0x0,0xbd4   ,0xbd4,0x1773   ,0x1774,0x22ae   ,0x22ad,
    0x2d4e   ,0x2d50,0x372a   ,0x372a,0x4013   ,0x4013,0x47e3   ,0x47e3,
    0x4e79   ,0x4e79,0x53b8   ,0x53b8,0x5787   ,0x5787,0x59d6   ,0x59d7,
    0x5a9d   ,0x5a9d,0x59d6   ,0x59d7,0x5786   ,0x5787,0x53b7   ,0x53b7,
    0x4e79   ,0x4e7a,0x47e4   ,0x47e3,0x4012   ,0x4013,0x372a   ,0x372a,
    0x2d4f   ,0x2d50,0x22ac   ,0x22ad,0x1773   ,0x1774,0xbd4   ,0xbd3,
    0x0   ,0x0,0xf42c   ,0xf42c,0xe88c   ,0xe88c,0xdd53   ,0xdd52,
    0xd2b1   ,0xd2b1,0xc8d6   ,0xc8d6,0xbfed   ,0xbfed,0xb81d   ,0xb81c,
    0xb187   ,0xb186,0xac49   ,0xac49,0xa87a   ,0xa879,0xa629   ,0xa62a,
    0xa563   ,0xa563,0xa629   ,0xa629,0xa879   ,0xa879,0xac49   ,0xac49,
    0xb186   ,0xb187,0xb81c   ,0xb81c,0xbfed   ,0xbfed,0xc8d6   ,0xc8d6,
    0xd2b2   ,0xd2b2,0xdd53   ,0xdd52,0xe88d   ,0xe88c,0xf42c   ,0xf42c,
    0xffff   ,0xffff,0xbd4   ,0xbd3,0x1774   ,0x1774,0x22ad   ,0x22ad,
    0x2d4e   ,0x2d4f,0x372a   ,0x3729,0x4013   ,0x4013,0x47e3   ,0x47e3,
    0x4e7a   ,0x4e79,0x53b7   ,0x53b8,0x5787   ,0x5786,0x59d7   ,0x59d7,
    0x5a9e   ,0x5a9d,0x59d7   ,0x59d7,0x5787   ,0x5786,0x53b8   ,0x53b7,
    0x4e79   ,0x4e7a,0x47e3   ,0x47e4,0x4013   ,0x4013,0x3729   ,0x372a,
    0x2d4f   ,0x2d4f,0x22ad   ,0x22ad,0x1774   ,0x1774,0xbd4   ,0xbd4,
    0x0   ,0x1,0xf42d   ,0xf42c,0xe88c   ,0xe88b,0xdd53   ,0xdd53,
    0xd2b1   ,0xd2b2,0xc8d7   ,0xc8d6,0xbfed   ,0xbfed,0xb81c   ,0xb81c,
    0xb187   ,0xb186,0xac48   ,0xac48,0xa879   ,0xa879,0xa629   ,0xa629,
    0xa563   ,0xa563,0xa629   ,0xa62a,0xa879   ,0xa879,0xac49   ,0xac49,
    0xb186   ,0xb187,0xb81d   ,0xb81c,0xbfed   ,0xbfed,0xc8d7   ,0xc8d6,
    0xd2b1   ,0xd2b1,0xdd53   ,0xdd54,0xe88c   ,0xe88c,0xf42c   ,0xf42c,
    0x0   ,0xffff,0xbd4   ,0xbd4,0x1773   ,0x1773,0x22ad   ,0x22ae,
    0x2d4f   ,0x2d4f,0x3729   ,0x372a,0x4013   ,0x4013,0x47e4   ,0x47e4,
    0x4e7a   ,0x4e79,0x53b7   ,0x53b7,0x5787   ,0x5788,0x59d6   ,0x59d6,
    0x5a9e   ,0x5a9d,0x59d7   ,0x59d7,0x5787   ,0x5786,0x53b8   ,0x53b7,
    0x4e7a   ,0x4e79,0x47e4   ,0x47e4,0x4013   ,0x4013,0x3729   ,0x372a,
    0x2d4f   ,0x2d4f,0x22ad   ,0x22ad,0x1774   ,0x1774,0xbd4   ,0xbd4,
    0x0   ,0xffff,0xf42c   ,0xf42c,0xe88c   ,0xe88d,0xdd52   ,0xdd53,
    0xd2b1   ,0xd2b1,0xc8d7   ,0xc8d6,0xbfed   ,0xbfed,0xb81c   ,0xb81d,
    0xb186   ,0xb186,0xac48   ,0xac49,0xa879   ,0xa879,0xa628   ,0xa629,
    0xa563   ,0xa563,0xa629   ,0xa62a,0xa879   ,0xa879,0xac48   ,0xac49,
    0xb186   ,0xb187,0xb81c   ,0xb81d,0xbfed   ,0xbfed,0xc8d6   ,0xc8d6,
    0xd2b1   ,0xd2b2,0xdd53   ,0xdd53,0xe88b   ,0xe88c,0xf42c   ,0xf42c,
    0xffff   ,0xffff,0xbd3   ,0xbd4,0x1774   ,0x1774,0x22ad   ,0x22ad,
    0x2d4f   ,0x2d4f,0x3729   ,0x372a,0x4012   ,0x4013,0x47e3   ,0x47e4,
    0x4e7a   ,0x4e7a,0x53b8   ,0x53b7,0x5787   ,0x5787,0x59d7   ,0x59d7,
    0x5a9d   ,0x5a9d,0x59d6   ,0x59d7,0x5787   ,0x5786,0x53b7   ,0x53b7,
    0x4e7a   ,0x4e79,0x47e4   ,0x47e4,0x4013   ,0x4013,0x372a   ,0x372a,
    0x2d4f   ,0x2d4f,0x22ad   ,0x22ad,0x1774   ,0x1774,0xbd4   ,0xbd3,
    0x0   ,0xffff,0xf42c   ,0xf42c,0xe88c   ,0xe88c,0xdd53   ,0xdd53,
    0xd2b2   ,0xd2b1,0xc8d7   ,0xc8d6,0xbfed   ,0xbfed,0xb81d   ,0xb81d,
    0xb187   ,0xb187,0xac48   ,0xac48,0xa87a   ,0xa879,0xa62a   ,0xa62a,
    0xa562   ,0xa563,0xa629   ,0xa629,0xa879   ,0xa879,0xac49   ,0xac48,
    0xb186   ,0xb186,0xb81d   ,0xb81c,0xbfee   ,0xbfee,0xc8d6   ,0xc8d7,
    0xd2b1   ,0xd2b1,0xdd53   ,0xdd53,0xe88c   ,0xe88c,0xf42c   ,0xf42c,
    0x1   ,0x0,0xbd4   ,0xbd4,0x1774   ,0x1774,0x22ac   ,0x22ae,
    0x2d4e   ,0x2d4f,0x372a   ,0x372a,0x4013   ,0x4013,0x47e4   ,0x47e4,
    0x4e79   ,0x4e79,0x53b8   ,0x53b7,0x5787   ,0x5787,0x59d7   ,0x59d7,
    0x5a9d   ,0x5a9c,0x59d6   ,0x59d7,0x5787   ,0x5787,0x53b8   ,0x53b7,
    0x4e78   ,0x4e7a,0x47e3   ,0x47e4,0x4013   ,0x4013,0x3729   ,0x3729,
    0x2d4f   ,0x2d4f,0x22ae   ,0x22ad,0x1774   ,0x1774,0xbd4   ,0xbd4,
    0x0   ,0x0,0xf42c   ,0xf42c,0xe88c   ,0xe88d,0xdd53   ,0xdd53,
    0xd2b1   ,0xd2b1,0xc8d7   ,0xc8d6,0xbfee   ,0xbfed,0xb81c   ,0xb81c,
    0xb187   ,0xb187,0xac49   ,0xac49,0xa879   ,0xa879,0xa629   ,0xa629,
    0xa563   ,0xa563,0xa628   ,0xa629,0xa879   ,0xa87a,0xac49   ,0xac48,
    0xb186   ,0xb186,0xb81d   ,0xb81d,0xbfec   ,0xbfed,0xc8d6   ,0xc8d6,
    0xd2b1   ,0xd2b1,0xdd54   ,0xdd53,0xe88d   ,0xe88b,0xf42b   ,0xf42c
};

static const uint16_t sineTable[320] = {
0x0000, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0xFFFF, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
0xFFFF, 0x0A03, 0x13C7, 0x1D0E, 0x259E, 0x2D41, 0x33C7, 0x3906,
0x3CDE, 0x3F36, 0x4000, 0x3F36, 0x3CDE, 0x3906, 0x33C7, 0x2D41,
0x259E, 0x1D0E, 0x13C7, 0x0A03, 0x0000, 0xF5FD, 0xEC39, 0xE2F2,
0xDA62, 0xD2BF, 0xCC39, 0xC6FA, 0xC322, 0xC0CA, 0xC000, 0xC0CA,
0xC322, 0xC6FA, 0xCC39, 0xD2BF, 0xDA62, 0xE2F2, 0xEC39, 0xF5FD,
};
#endif

const int fm_use_analog_input = 1; //from libaudiosetting.so
const int fm_chip = 2;//from libaudiosetting.so

#define FOR_FM 3
#define FORCE_HEADPHONES 2
#define FORCE_NONE 0

#if 0
// Define I2S related functions to make switch between fake and real easy

static void* I2SGetInstance()
{
    LOGD("I2SGetInstance");
#ifdef FAKE_FM
    return (void*)sineTable;              //a fake address, we do not use this handle in fake case
#else
    return (void*)AudioI2S::getInstance();
#endif
}

static void I2SFreeInstance(void *handle)
{
    LOGD("I2SFreeInstance");
#ifdef FAKE_FM
    return;              //Just return
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    i2sHandle->freeInstance();
#endif
}

static uint32 I2SOpen(void* handle)
{
    LOGD("I2SOpen");
#ifdef FAKE_FM
    return 1;                         //a fake id
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->open();
#endif
}


static bool I2SSet(void* handle, I2STYPE type)
{
    LOGD("I2SSet");
#ifdef FAKE_FM
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->set(type);
#endif
}


static bool I2SClose(void* handle, uint32_t Identity)
{
    LOGD("I2SClose");
#ifdef FAKE_FM
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->close(Identity);
#endif
}

static bool I2SStart(void* handle, uint32_t Identity,I2STYPE type)
{
    LOGD("I2SStart");
#ifdef FAKE_FM
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->start(Identity,type);
#endif
}

static bool I2SStop(void* handle, uint32_t Identity,I2STYPE type)
{
    LOGD("I2SStop");
#ifdef FAKE_FM
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->stop(Identity,type);
#endif
}

static uint32 I2SGetReadBufferSize(void* handle)
{
#ifdef FAKE_FM
    return 384 * sizeof(uint16_t) * 2;        // 2 x sine table size
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->GetReadBufferSize();
#endif
}

static uint32 I2SRead(void* handle, uint32_t Identity,void* buffer, uint32 buffersize)
{
#ifdef FAKE_FM
    usleep(1000);
    int sineTableSize = sineTable48KSIZE*sizeof(uint16_t);
    char * ptr = (char*)buffer;
    memcpy(ptr, sineTable48K, sineTableSize);
    ptr += sineTableSize;
    memcpy(ptr, sineTable48K, sineTableSize);
    ptr += sineTableSize;

    return sineTableSize * 2;

#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->read(Identity, buffer, buffersize);
#endif

}
#endif

// TODO: Determine appropriate return codes
static status_t ERROR_NOT_OPEN = -1;
static status_t ERROR_OPEN_FAILED = -2;
static status_t ERROR_ALLOCATE_FAILED = -4;
static status_t ERROR_NOT_SUPPORTED = -8;
static status_t ERROR_NOT_READY = -16;
static status_t ERROR_START_FAILED = -32;
static status_t ERROR_STOP_FAILED = -64;
static status_t STATE_INIT = 0;
static status_t STATE_ERROR = 1;
static status_t STATE_OPEN = 2;

String8  ANALOG_FM_ENABLE =(String8)("AudioSetFmEnable=1");
String8  ANALOG_FM_DISABLE =(String8)("AudioSetFmEnable=0");

String8  DIGITAL_FM_ENABLE =(String8)("AudioSetFmDigitalEnable=1");
String8  DIGITAL_FM_DISABLE =(String8)("AudioSetFmDigitalEnable=0");

#define AUDIO_STREAM_FM 10

FMAudioPlayer::FMAudioPlayer() :
    mAudioBuffer(NULL), mPlayTime(-1), mDuration(-1), mState(STATE_ERROR),
    mStreamType(AUDIO_STREAM_FM),
    mExit(false), mPaused(false), mRender(false), mRenderTid(-1), mI2Sid(0), mI2Sdriver(NULL), mI2SStartFlag(0),mDataType(-1)
{
    LOGD("[%d]FMAudioPlayer constructor\n", mI2Sid);
    if (fm_use_analog_input == 1)
    {
        LOGD("FM use analog input");
    }

#if 0
    else if(fm_use_analog_input == 0)
    {
       mI2Sdriver = I2SGetInstance();
       if ( mI2Sdriver == NULL ){
        LOGE("I2S driver doesn't exists\n");
       }
    }
#endif

    mMutePause = 0;
}

void FMAudioPlayer::onFirstRef()
{
    LOGD("onFirstRef");

    // create playback thread
    Mutex::Autolock l(mMutex);

    if (fm_use_analog_input == 1)
    {
      LOGD("FMAudioPlayer use analog input - onFirstRef");
      mRenderTid =1;
      mState = STATE_INIT;
    }
#if 0
    else if (fm_use_analog_input == 0)
    {
       createThreadEtc(renderThread, this, "FM audio player", ANDROID_PRIORITY_AUDIO);
       mCondition.waitRelative(mMutex,seconds (3));
       if (mRenderTid > 0) {
        LOGD("[%d]render thread(%d) started", mI2Sid, mRenderTid);
        mState = STATE_INIT;
       }
    }
#endif

}

status_t FMAudioPlayer::initCheck()
{
    if (mState != STATE_ERROR) return NO_ERROR;
    return ERROR_NOT_READY;
}

FMAudioPlayer::~FMAudioPlayer() {
    LOGD("[%d]FMAudioPlayer destructor\n", mI2Sid);
    release();
    if (fm_use_analog_input == 1)
      LOGD("FMAudioPlayer use analog input - destructor end\n");
#if 0
    else if (fm_use_analog_input == 0)
    {
      //free I2S instance
      I2SFreeInstance(mI2Sdriver);
      mI2Sdriver = NULL;
      LOGD("[%d]FMAudioPlayer destructor end\n", mI2Sid);
    }
#endif

}

status_t FMAudioPlayer::setDataSource(
        const char* path, const KeyedVector<String8, String8> *)
{
    LOGD("FMAudioPlayer setDataSource path=%s \n",path);
    return setdatasource(path, -1, 0, 0x7ffffffffffffffLL); // intentionally less than LONG_MAX
}

status_t FMAudioPlayer::setDataSource(int fd, int64_t offset, int64_t length)
{
    LOGD("FMAudioPlayer setDataSource offset=%d, length=%d \n",((int)offset),((int)length));
    return setdatasource(NULL, fd, offset, length);
}


status_t FMAudioPlayer::setdatasource(const char *path, int fd, int64_t offset, int64_t length)
{
    LOGD("[%d]setdatasource",mI2Sid);

    // file still open?
    Mutex::Autolock l(mMutex);
    if (mState == STATE_OPEN) {
        reset_nosync();
    }

    if (fm_use_analog_input == 1)
    {
      mState = STATE_OPEN;
      return NO_ERROR;
     }
#if 0
    else if (fm_use_analog_input == 0)
    {
      //already opend
      if (mI2Sdriver && mI2Sid!=0) {
       mState = STATE_OPEN;
       return NO_ERROR;
      }


      // get ID
      mI2Sid = I2SOpen(mI2Sdriver);
      if ( mI2Sid == 0 ){
        LOGE("I2S driver get ID fail\n");
        mState = STATE_ERROR;
        return ERROR_OPEN_FAILED;
      }
      //set FM
      /*
      if ( !I2SSet(mI2Sdriver,FMRX) ){
        LOGE("I2S driver set MATV fail\n");
        mState = STATE_ERROR;
        return ERROR_OPEN_FAILED;
      }*/
      LOGD("Finish initial I2S driver, instance:%p, ID:%d", mI2Sdriver, mI2Sid);

      mState = STATE_OPEN;
      return NO_ERROR;

    }
#endif

    return NO_ERROR;
}

status_t FMAudioPlayer::prepare()
{
    LOGD("[%d]prepare\n", mI2Sid);

    if (mState != STATE_OPEN ) {
        LOGE("prepare ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }
    return NO_ERROR;
}

status_t FMAudioPlayer::prepareAsync() {
    LOGD("[%d]prepareAsync\n", mI2Sid);

    // can't hold the lock here because of the callback
    // it's safe because we don't change state
    if (mState != STATE_OPEN ) {
        sendEvent(MEDIA_ERROR);
        LOGD("prepareAsync sendEvent(MEDIA_ERROR) \n");
        return NO_ERROR;
    }
    sendEvent(MEDIA_PREPARED);
    return NO_ERROR;
}

status_t FMAudioPlayer::start()
{
    LOGD("[%d]start\n", mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        LOGE("start ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

    if (fm_use_analog_input == 1)
    {
#if 0
      if (fm_chip == 1)
      {
       {
       int a = 7393;
       int b = 739;
       int c = 73939;
       int in, out;
       //sp<IATVCtrlService> spATVCtrlService;
       sp<IServiceManager> sm = defaultServiceManager();
       sp<IBinder> binder;
       do{
          binder = sm->getService(String16("media.ATVCtrlService"));
          if (binder != 0)
             break;

          LOGW("ATVCtrlService not published, waiting...");
          usleep(500000); // 0.5 s
       } while(true);
       spATVCtrlService = interface_cast<IATVCtrlService>(binder);
       in = (int)&a;
       out = spATVCtrlService->ATVCS_matv_set_parameterb(in);
       if ( out != (in+a)*b + c ) {
          LOGD("Set Parameberb failed %d", out);
          return ERROR_NOT_OPEN;
        }
       }

       ///if(spATVCtrlService)
       {
        LOGE("FM Set 5192 Line in");
        spATVCtrlService->ATVCS_matv_set_chipdep(190,3);
       }
      }
#endif
     AudioSystem::setParameters (0,ANALOG_FM_ENABLE);
     AudioSystem::setForceUse (static_cast <audio_policy_force_use_t>(FOR_FM), static_cast <audio_policy_forced_cfg_t>(FORCE_HEADPHONES));

     mPaused = false;
     mRender = true;

    }
#if 0
    else if (fm_use_analog_input == 0)
    {

      if ( mMutePause == true) {
       mMutePause = false;
       mPaused = false;
       mAudioSink->setVolume (1.0,1.0);
      }

      if (fm_chip == 1)
      {
          /*
          if ( !I2SSet(mI2Sdriver,MATV) ){
              LOGE("I2S driver set MATV fail\n");
              mState = STATE_ERROR;
              return ERROR_OPEN_FAILED;
          }
          */
      }
      else
      {
          /*
          if ( !I2SSet(mI2Sdriver,FMRX) ){
              LOGE("I2S driver set FMRX fail\n");
              mState = STATE_ERROR;
              return ERROR_OPEN_FAILED;
          }
          */
      }

      // Start I2S driver
      if(!mI2SStartFlag) {
          if (fm_chip == 1){
              if(!I2SStart(mI2Sdriver,mI2Sid,MATV)){
                  LOGE("I2S start fialed");
                  reset_nosync();
                  return ERROR_START_FAILED;
              }
              mDataType = MATV;
          }

          else{
              if(!I2SStart(mI2Sdriver,mI2Sid,FMRX)){
                  LOGE("I2S start fialed");
                  reset_nosync();
                  return ERROR_START_FAILED;
              }
              mDataType = FMRX;
          }
          mI2SStartFlag = 1;
      }
     mPaused = false;
     mRender = true;

     //AudioSystem::setParameters (0,DIGITAL_FM_ENABLE);

     // wake up render thread
     LOGD("start wakeup render thread\n");
     mCondition.signal();
     }
#endif

    return NO_ERROR;
}

status_t FMAudioPlayer::stop()
{
    LOGD("[%d]stop\n",mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        LOGE("stop ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

    if (fm_use_analog_input == 1)
    AudioSystem::setParameters (0,ANALOG_FM_DISABLE);
    AudioSystem::setForceUse (static_cast <audio_policy_force_use_t>(FOR_FM), static_cast <audio_policy_forced_cfg_t>(FORCE_NONE));

#if 0
    else if (fm_use_analog_input == 0)
    {
      // stop I2S driver
      if(mI2SStartFlag == 1){                    //only when I2S is open
       if(!I2SStop(mI2Sdriver,mI2Sid,(I2STYPE)mDataType)){
           LOGE("I2S stop fialed");
           reset_nosync();
           return ERROR_STOP_FAILED;
       }
       mI2SStartFlag = 0;
      }
     AudioSystem::setParameters (0,DIGITAL_FM_DISABLE);

    }
#endif

    mPaused = true;
    mRender = false;
    return NO_ERROR;
}

status_t FMAudioPlayer::seekTo(int position)
{
    LOGD("[%d]seekTo %d\n", position, mI2Sid);
    Mutex::Autolock l(mMutex);
    return NO_ERROR;
}

status_t FMAudioPlayer::pause()
{
    LOGD("[%d]pause\n",mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        LOGD("pause ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }
    LOGD("pause got lock\n");
    if (fm_use_analog_input == 1)
    {
        if ( mMutePause == false) {
            mMutePause = true;
            AudioSystem::setParameters (0,ANALOG_FM_DISABLE);
        }
    }
#if 0
    else if (fm_use_analog_input == 0)
    {
        if( mMutePause == false) {
            mMutePause = true;
            mAudioSink->setVolume (0.0 , 0.0);
        }
    }
#endif

    mPaused = true;
    return NO_ERROR;
}

bool FMAudioPlayer::isPlaying()
{
    LOGD("[%d]isPlaying\n",mI2Sid);
    if (mState == STATE_OPEN) {
        return mRender;
    }
    return false;
}

status_t FMAudioPlayer::getCurrentPosition(int* position)
{
    LOGD("[%d]getCurrentPosition always return 0\n", mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        LOGD("getCurrentPosition(): file not open");
        return ERROR_NOT_OPEN;
    }
    *position = 0;
    return NO_ERROR;
}

status_t FMAudioPlayer::getDuration(int* duration)
{
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        LOGD("getDuration ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

    *duration = 1000;
    LOGD("[%d]getDuration duration, always return 0 \n",mI2Sid);
    return NO_ERROR;
}

status_t FMAudioPlayer::release()
{
    LOGD("[%d]release\n",mI2Sid);

    int ret =0;
    int count = 100;
    LOGD("release mMutex.tryLock ()");
    do{
        ret = mMutex.tryLock ();
        if(ret){
            LOGW("FMAudioPlayer::release() mMutex return ret = %d",ret);
            usleep(20*1000);
            count --;
        }
    }while(ret && count);  // only cannot lock

    reset_nosync();
    if (fm_use_analog_input == 1){
        LOGD("FMAudioPlayer use analog input");
    }
#if 0
    else if (fm_use_analog_input == 0)
    {
        // TODO: timeout when thread won't exit, wait for render thread to exit
        if (mRenderTid > 0) {
            mExit = true;
            LOGD("release signal \n");
            mCondition.signal();
            LOGD("release wait \n");
            mCondition.waitRelative(mMutex,seconds (3));
        }
    }
#endif

    mMutex.unlock ();
    return NO_ERROR;
}

status_t FMAudioPlayer::reset()
{
    LOGD("[%d]reset\n", mI2Sid);
    Mutex::Autolock l(mMutex);
    return reset_nosync();
}

// always call with lock held
status_t FMAudioPlayer::reset_nosync()
{
    LOGD("[%d]reset_nosync start\n",mI2Sid);

    if (fm_use_analog_input == 1){
        LOGD("FMAudioPlayer use analog input");
        AudioSystem::setParameters (0,ANALOG_FM_DISABLE);//Add by Changqing
    }
#if 0
    else if (fm_use_analog_input == 0)
    {
        // ToDo: close I2S driver
        if (mI2Sdriver && mI2Sid!=0) {
            I2SStop(mI2Sdriver,mI2Sid,(I2STYPE)mDataType);
            I2SClose(mI2Sdriver,mI2Sid);
        }
        mI2Sid = 0;
     AudioSystem::setParameters (0,DIGITAL_FM_DISABLE);//Add by Changqing
    }
#endif

    mI2SStartFlag = 0;
    mState = STATE_ERROR;
    mPlayTime = -1;
    mDuration = -1;
    mPaused = false;
    mRender = false;
    LOGD("[%d]reset_nosync end\n",mI2Sid);
    return NO_ERROR;
}

status_t FMAudioPlayer::setLooping(int loop)
{
    LOGD("[%d]setLooping, do nothing \n",mI2Sid);
    return NO_ERROR;
}

#define FM_AUDIO_CHANNEL_NUM      2

status_t FMAudioPlayer::createOutputTrack() {
    // base on configuration define samplerate .
    int FM_AUDIO_SAMPLING_RATE;
    if (fm_chip == 2){
        FM_AUDIO_SAMPLING_RATE = 48000;
    }
    else{
        FM_AUDIO_SAMPLING_RATE = 32000;
    }
    LOGD("[%d]Create AudioTrack object: rate=%d, channels=%d\n",mI2Sid, FM_AUDIO_SAMPLING_RATE, FM_AUDIO_CHANNEL_NUM);

    // open audio track
    if (mAudioSink->open(FM_AUDIO_SAMPLING_RATE, FM_AUDIO_CHANNEL_NUM, AUDIO_FORMAT_PCM_16_BIT, 3) != NO_ERROR) {
        LOGE("mAudioSink open failed");
        return ERROR_OPEN_FAILED;
    }
    return NO_ERROR;
}

int FMAudioPlayer::renderThread(void* p) {
    return ((FMAudioPlayer*)p)->render();
}

//#define AUDIOBUFFER_SIZE 4096
int FMAudioPlayer::render() {
    int result = -1;
    int temp;
    int current_section = 0;
    bool audioStarted = false;
    bool firstOutput = false;
    int t_result = -1;
    int bufSize = 0;
    int lastTime = 0;
    int thisTime = 0;
    int dataCount = 0;
    int frameCount = 0;


#ifdef FM_AUDIO_FILELOG
   FILE *fp;
   fp = fopen("sdcard/test.pcm","wb");
   LOGD("fp:%d", fp);
#endif

#if 0
    bufSize = I2SGetReadBufferSize(mI2Sdriver);
    LOGD("got buffer size = %d", bufSize);
    mAudioBuffer = new char[bufSize*2];
    mDummyBuffer = new char[bufSize*2];
    memset(mDummyBuffer, 0, bufSize);

    LOGD("mAudioBuffer: %p \n",mAudioBuffer);
    if (!mAudioBuffer) {
        LOGD("mAudioBuffer allocate failed\n");
        goto threadExit;
    }
    // if set prority false , force to set priority
    if(t_result == -1)
    {
       struct sched_param sched_p;
       sched_getparam(0, &sched_p);
       sched_p.sched_priority = RTPM_PRIO_FM_AUDIOPLAYER ;
       if(0 != sched_setscheduler(0, SCHED_RR, &sched_p))
       {
          LOGE("[%s] failed, errno: %d", __func__, errno);
       }
       else
       {
          sched_p.sched_priority = RTPM_PRIO_FM_AUDIOPLAYER;
          sched_getparam(0, &sched_p);
          LOGD("sched_setscheduler ok, priority: %d", sched_p.sched_priority);
       }
    }

    // let main thread know we're ready
    {
        int ret =0;
        int count = 100;
        LOGD("render mMutex.tryLock ()");
        do{
            ret = mMutex.tryLock ();
            if(ret){
                LOGW("FMAudioPlayer::release() mMutex return ret = %d",ret);
                usleep(20*1000);
                count --;
            }
        }while(ret && count);  // only cannot lock

        mRenderTid = myTid();
        LOGD("[%d]render start mRenderTid=%d\n",mI2Sid, mRenderTid);
        mCondition.signal();
        mMutex.unlock ();
    }

    while (1) {
        long numread = 0;
        {
            Mutex::Autolock l(mMutex);

            // pausing?
            if (mPaused) {
                LOGD("render - pause\n");
                if (mAudioSink->ready()){
                    mAudioSink->pause();
                    mAudioSink->flush();
                }
                usleep(300* 1000);
                mRender = false;
                audioStarted = false;
            }

            // nothing to render, wait for client thread to wake us up
            if (!mExit && !mRender) {
                LOGD("render - signal wait\n");
                mCondition.wait(mMutex);
                frameCount = 0;
                LOGD("render - signal rx'd\n");
            }

            if (mExit) break;

            // We could end up here if start() is called, and before we get a
            // chance to run, the app calls stop() or reset(). Re-check render
            // flag so we don't try to render in stop or reset state.
            if (!mRender) {
                continue;
           }

            if (!mAudioSink->ready()) {
                 LOGD("render - create output track\n");
                 if (createOutputTrack() != NO_ERROR)
                     break;
            }
        }

        // codec returns negative number on error
        if (numread < 0) {
            LOGE("Error in FMPlayer  numread=%ld",numread);
            sendEvent(MEDIA_ERROR);
            break;
        }

        // start audio output if necessary
        if (!audioStarted && !mPaused && !mExit) {
            LOGD("render - starting audio\n");
            mAudioSink->start();
            // setparameter to hardware after staring, for cr ALPS00073272
            AudioSystem::setParameters (0,DIGITAL_FM_ENABLE);
            audioStarted = true;
            firstOutput = true;

            //firstly push some amount of buffer to make the mixer alive
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               LOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               LOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               LOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }

        }

        {
            Mutex::Autolock l(mMutex);
            int brt =0, art =0;
            //LOGD("[%lld] before read %d",brt=getTimeMs());
            if (firstOutput) {
               numread = I2SRead(mI2Sdriver, mI2Sid, mAudioBuffer, bufSize);
               firstOutput = false;
            }
            else {
                 numread = I2SRead(mI2Sdriver, mI2Sid, mAudioBuffer, bufSize);
            }
            //LOGD("[%lld] after read %d",art=getTimeMs());
            if(art-brt > 90 )
               LOGW("read time abnormal");
            //LOGD("[%d]read %d bytes from I2S, id:%d",&mI2Sid, numread, mI2Sid);

            frameCount++;
        }

        lastTime = thisTime;
        thisTime = getTimeMs();
        if ( thisTime -lastTime > 160 )
           LOGW(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!time diff = %d", thisTime -lastTime);

        // Write data to the audio hardware
        dataCount+=numread;
        //LOGD("[%lld] FMAudioPlayer read data count: %d",getTimeMs(), dataCount);

       if ((temp = mAudioSink->write(mAudioBuffer, numread)) < 0) {
           LOGE("Error in writing:%d",temp);
           result = temp;
           break;
       }
       //LOGD("[%lld] after write writecount = %d" ,getTimeMs(),temp);
       //sleep to allow command to get mutex
       usleep(1000);
    }

threadExit:
    mAudioSink.clear();


    if (mAudioBuffer) {
        delete [] mAudioBuffer;
        mAudioBuffer = NULL;
    }
    if (mDummyBuffer) {
        delete [] mDummyBuffer;
        mDummyBuffer = NULL;
    }

    LOGD("[%d]render end mRenderTid=%d\n",mI2Sid, mRenderTid);

    // tell main thread goodbye
    Mutex::Autolock l(mMutex);
    mRenderTid = -1;
    mCondition.signal();
#endif
#ifdef FM_AUDIO_FILELOG
   fclose(fp);
#endif
    return result;
}



} // end namespace android

//#endif
