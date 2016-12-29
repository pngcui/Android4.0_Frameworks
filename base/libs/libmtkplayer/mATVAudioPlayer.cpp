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
#ifdef MTK_MATV_SUPPORT

//#define LOG_NDEBUG 0
#define LOG_TAG "mATVPlayer"
#include "utils/Log.h"
#include "cutils/xlog.h"

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <media/AudioSystem.h>
#include <utils/String8.h>
#include <system/audio.h>
extern "C" {
#include "kal_release.h"
//#include "matvctrl.h"
}

#include <binder/IServiceManager.h>

#include "AudioYusuDef.h"
#include "AudioI2S.h"
#include "mATVAudioPlayer.h"
#include <linux/rtpm_prio.h>

#ifdef MTK_PLATFORM_MT6516
#include "AudioYusuIoctl.h"
#else
#include "AudioIoctl.h"
#endif

#include "AudioYusuHardware.h"

#define MUTE_PAUSE
//#define MATV_AUDIO_LINEIN_PATH
//#define ATV_AUDIO_FILELOG

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

//#define FAKE_MATV
//sine table for simulation only
#ifdef FAKE_MATV
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

extern const int matv_use_analog_input;//from libaudiosetting.so

// Define I2S related functions to make switch between fake and real easy
static void* I2SGetInstance()
{
    SXLOGD("I2SGetInstance");
#ifdef FAKE_MATV
    return (void*)sineTable;              //a fake address, we do not use this handle in fake case
#else
    return (void*)AudioI2S::getInstance();
#endif
}

static void I2SFreeInstance(void *handle)
{
    SXLOGD("I2SFreeInstance");
#ifdef FAKE_MATV
    return;              //Just return
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    i2sHandle->freeInstance();
#endif
}

static uint32 I2SOpen(void* handle)
{
    SXLOGD("I2SOpen");
#ifdef FAKE_MATV
    return 1;                         //a fake id
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->open();
#endif
}
/*
static bool I2SSet(void* handle, I2STYPE type)
{
    SXLOGD("I2SSet");
#ifdef FAKE_MATV
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->set(type);
#endif
}*/

static bool I2SClose(void* handle, uint32_t Identity)
{
    SXLOGD("I2SClose");
#ifdef FAKE_MATV
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->close(Identity);
#endif
}

static bool I2SStart(void* handle, uint32_t Identity, I2STYPE type)
{
    SXLOGD("I2SStart");
#ifdef FAKE_MATV
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->start(Identity,type);
#endif
}

static bool I2SStop(void* handle, uint32_t Identity,I2STYPE type)
{
    SXLOGD("I2SStop");
#ifdef FAKE_MATV
    return 1;                         //always return ture
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->stop(Identity,type);
#endif
}

static uint32 I2SGetReadBufferSize(void* handle)
{
#ifdef FAKE_MATV
    return 320 * sizeof(uint16_t) * 2;        // 2 x sine table size
#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->GetReadBufferSize();
#endif
}

static uint32 I2SRead(void* handle, uint32_t Identity,void* buffer, uint32 buffersize)
{
#ifdef FAKE_MATV
    usleep(1000);
    int sineTableSize = 320*sizeof(uint16_t);
    char * ptr = (char*)buffer;
    memcpy(ptr, sineTable, sineTableSize);
    ptr += sineTableSize;
    memcpy(ptr, sineTable, sineTableSize);
    ptr += sineTableSize;

    return sineTableSize * 2;

#else
    AudioI2S* i2sHandle = (AudioI2S*)handle;
    return i2sHandle->read(Identity, buffer, buffersize);
#endif

}

// Here start the code for I2S test for real MT5192
// Pure test use
//#define MATV_AUDIO_TEST_I2S

#ifdef MATV_AUDIO_TEST_I2S

int tvscan_finish = 0;

extern "C" {
static void atv_autoscan_progress_cb(void* cb_param, kal_uint8 precent,kal_uint8 ch,kal_uint8 chnum)
{
    matv_chscan_state scan_state;
    SXLOGD("audio scan call back");
    matv_chscan_query(&scan_state);
    SXLOGD("CB.autoscan_progress: %d%% ,update CH-%02d(%c)\n",
            precent,scan_state.ch_latest_updated,
            scan_state.updated_entry.flag?'O':'X'
            );
    //tvscan_progress = precent;
}
static void atv_fullscan_progress_cb(void* cb_param, kal_uint8 precent,kal_uint32 freq,kal_uint32 freq_start,kal_uint32 freq_end){
    SXLOGD("CB.fullscan_progress: %d%%\n",precent);
    //tvscan_progress = precent;
}


static void atv_scanfinish_cb(void* cb_param, kal_uint8 chnum){
    SXLOGD("CB.scanfinish: chnum:%d\n",chnum);
    tvscan_finish=1;
}

static void atv_audioformat_cb(void* cb_param, kal_uint32 format){
    SXLOGD("CB.audioformat: %08x\n",format);
}

}

static int matv_ts_init()
{
    int ret;
    tvscan_finish = 0;
    ret = matv_init();
    matv_register_callback(0,
                           atv_autoscan_progress_cb,
                           atv_fullscan_progress_cb,
                           atv_scanfinish_cb,
                           atv_audioformat_cb);

    return (ret);
}

static void startMATVChip()
{
    int ret;
    matv_ch_entry ch_ent;

    SXLOGD(" startMATVChip begin");

    // init
    ret = matv_ts_init();
    SXLOGD(" mATV init result:%d", ret);

    // force to some specific channel

    matv_set_country(TV_TAIWAN);
    ch_ent.freq = 687250;
    ch_ent.sndsys = 1;
    ch_ent.colsys = 3;
    ch_ent.flag = 1;
    matv_set_chtable(50, &ch_ent);
    matv_get_chtable(50, &ch_ent);
    matv_change_channel(50);
    SXLOGD("channel:%d, freq:%d, sndsys:%d, colsys:%d, flag:%d \n",50,ch_ent.freq, ch_ent.sndsys, ch_ent.colsys, ch_ent.flag);
    return;


    // scan channel
    SXLOGD(" Start Channel Scan ...");
    matv_set_country(TV_TAIWAN);
    matv_chscan(MATV_AUTOSCAN);
    while(!tvscan_finish)
        sleep(1);
    SXLOGD(" Finiah Channel Scan ...");

    //getting channel number
    int i=1;
    int ch_candidate = 0;
    int validChannel[128];
    int validIndex=0;
    while(matv_get_chtable(i++,&ch_ent))
    {
        if(ch_ent.flag&CH_VALID)
        {
            SXLOGD("channel:%d, freq:%d, sndsys:%d, colsys:%d, flag:%d \n",i-1,ch_ent.freq, ch_ent.sndsys, ch_ent.colsys, ch_ent.flag);
            ch_candidate=i-1;
            validChannel[validIndex++] = ch_candidate;
        }
    }

    // set to last channel number
    SXLOGD("Last channel number is:%d", ch_candidate);
    matv_change_channel(50);


    /*
    SXLOGD("Turn on analog audio");
    matv_set_chipdep(190,0);//Turn on analog audio

    int TotalChannelNum = validIndex-1;
    matv_audio_play();
    for ( i = 0 ; i < TotalChannelNum ; i++)  {
       matv_change_channel(validChannel[i]);
        SXLOGD("changed to channel:%d", validChannel[i]);
        usleep(10000000);
    }
    matv_audio_stop();
    */

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

mATVAudioPlayer::mATVAudioPlayer() :
    mAudioBuffer(NULL), mPlayTime(-1), mDuration(-1), mState(STATE_ERROR),
    mStreamType(AUDIO_STREAM_MATV),
    mExit(false), mPaused(false), mRender(false), mRenderTid(-1), mI2Sid(0), mI2Sdriver(NULL), mI2SStartFlag(0), mMutePause(0)
{
    SXLOGD("[%d]mATVAudioPlayer constructor\n", mI2Sid);

    if (matv_use_analog_input == 1)
    {
       SXLOGD("line in path => constructor, Not open I2S");
       return;
    }

    mI2Sdriver = I2SGetInstance();
    if ( mI2Sdriver == NULL ){
        SXLOGE("I2S driver doesn't exists\n");
    }

    mFd = ::open(kAudioDeviceName, O_RDWR);
    if ( mFd < 0 ){
        SXLOGE("cannot open audio driver\n");
    }

}

void mATVAudioPlayer::onFirstRef()
{
    SXLOGD("onFirstRef");

    if (matv_use_analog_input == 1)
    {
       SXLOGD("line in path => onFirstRef, Not create thread");
       mState = STATE_INIT;
       return;
    }

    // create playback thread
    Mutex::Autolock l(mMutex);
    createThreadEtc(renderThread, this, "mATV audio player", ANDROID_PRIORITY_AUDIO);
    mCondition.wait(mMutex);
    if (mRenderTid > 0) {
        SXLOGD("[%d]render thread(%d) started", mI2Sid, mRenderTid);
        mState = STATE_INIT;
    }

}

status_t mATVAudioPlayer::initCheck()
{
    if (mState != STATE_ERROR) return NO_ERROR;
    return ERROR_NOT_READY;
}

mATVAudioPlayer::~mATVAudioPlayer() {
    SXLOGD("[%d]mATVAudioPlayer destructor\n", mI2Sid);
    release();

    if (matv_use_analog_input == 1)
    {
       SXLOGD("line in path => destructor, Not free I2S");
       return;
    }

    //free I2S instance
    I2SFreeInstance(mI2Sdriver);
    mI2Sdriver = NULL;

    if (mFd >= 0) ::close(mFd);

    SXLOGD("[%d]mATVAudioPlayer destructor end\n", mI2Sid);

}

status_t mATVAudioPlayer::setDataSource(
        const char* path, const KeyedVector<String8, String8> *)
{
    SXLOGD("mATVAudioPlayer setDataSource path=%s \n",path);
    return setdatasource(path, -1, 0, 0x7ffffffffffffffLL); // intentionally less than LONG_MAX
}

status_t mATVAudioPlayer::setDataSource(int fd, int64_t offset, int64_t length)
{
    SXLOGD("mATVAudioPlayer setDataSource offset=%d, length=%d \n",((int)offset),((int)length));
    return setdatasource(NULL, fd, offset, length);
}


status_t mATVAudioPlayer::setdatasource(const char *path, int fd, int64_t offset, int64_t length)
{
    SXLOGD("[%d]setdatasource",mI2Sid);

    // file still open?
    Mutex::Autolock l(mMutex);
    if (mState == STATE_OPEN) {
        reset_nosync();
    }

    //Check mATV Service for bounding
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

          SXLOGW("ATVCtrlService not published, waiting...");
          usleep(500000); // 0.5 s
       } while(true);
       spATVCtrlService = interface_cast<IATVCtrlService>(binder);
       in = (int)&a;
       out = spATVCtrlService->ATVCS_matv_set_parameterb(in);
       if ( out != (in+a)*b + c ) {
          SXLOGD("Set Parameberb failed");
          return ERROR_OPEN_FAILED;
       }
    }

    if (matv_use_analog_input == 1)
    {
       SXLOGD("line in path => setdatasource");
       mState = STATE_OPEN;
       return NO_ERROR;
    }

    //I2S already opened
    if (mI2Sdriver && mI2Sid!=0) {
       mState = STATE_OPEN;
       return NO_ERROR;
    }
    // open I2S driver
    mI2Sid = I2SOpen(mI2Sdriver);
    if ( mI2Sid == 0 ){
        SXLOGE("I2S driver get ID fail\n");
        mState = STATE_ERROR;
        return ERROR_OPEN_FAILED;
    }
    //set MATV
    /*
    if ( !I2SSet(mI2Sdriver,MATV) ){
        SXLOGE("I2S driver set MATV fail\n");
        mState = STATE_ERROR;
        return ERROR_OPEN_FAILED;
    }
    */
    SXLOGD("Finish initial I2S driver, instance:%p, ID:%d", mI2Sdriver, mI2Sid);

    mState = STATE_OPEN;
    return NO_ERROR;
}

status_t mATVAudioPlayer::prepare()
{
    SXLOGD("[%d]prepare\n", mI2Sid);

    if (mState != STATE_OPEN ) {
        SXLOGE("prepare ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }
    return NO_ERROR;
}

status_t mATVAudioPlayer::prepareAsync() {
    SXLOGD("[%d]prepareAsync\n", mI2Sid);

    // can't hold the lock here because of the callback
    // it's safe because we don't change state
    if (mState != STATE_OPEN ) {
        sendEvent(MEDIA_ERROR);
        SXLOGD("prepareAsync sendEvent(MEDIA_ERROR) \n");
        return NO_ERROR;
    }
    sendEvent(MEDIA_PREPARED);
    return NO_ERROR;
}

status_t mATVAudioPlayer::start()
{
    SXLOGD("[%d]start\n", mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        SXLOGE("start ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

 // Start MATV chip audio
    SXLOGD("Start matv audio play");
    spATVCtrlService->ATVCS_matv_audio_play();
    //matv_audio_play();

    //usleep(10*1000*1000);

    if (matv_use_analog_input == 1)
    {
       SXLOGD("line in path => start");
       // tell Audio Driver we are going to stop: for Daul Speaker (ClassD + ClassAB)
      if (mFd >= 0 ) {
      int Reg_Data[3] = {INFO_U2K_MATV_AUDIO_START, 0, 0};
      ::ioctl(mFd,YUSU_INFO_FROM_USER,&Reg_Data);
      }
      spATVCtrlService->ATVCS_matv_set_chipdep(190,3);
      AudioSystem::setParameters(0, (String8)"AtvAudioLineInEnable=1");
      AudioSystem::setStreamMute(AUDIO_STREAM_MATV, false);
      return NO_ERROR;
    }

    ///for issue: fm tx i2s and matv i2s can work at same time, so it need wait fm tx stop.
    static String8 GetFmTxEnable_false = String8("GetFmTxEnable=false");
    bool fmtxisenable = false;
    int fmtxcnt = 0;
    String8 keyValuePairs = AudioSystem::getParameters(0, (String8)"GetFmTxEnable");
    if(keyValuePairs.compare (GetFmTxEnable_false) == 0)
    {
        fmtxisenable = false;
    }
    else
        fmtxisenable = true;

    while(fmtxisenable)
    {
        String8 keyValuePairs = AudioSystem::getParameters(0, (String8)"GetFmTxEnable");
        if(keyValuePairs.compare (GetFmTxEnable_false) == 0)
        {
            fmtxisenable = false;
        }
        else
            fmtxisenable = true;

        usleep(10* 1000);
        fmtxcnt++;
        if(fmtxcnt>50)
            return ERROR_START_FAILED;
    }


#ifdef MUTE_PAUSE
    if ( mMutePause == true) {
       //mAudioSink->mute(0);
       mMutePause = false;
       return NO_ERROR;
    }
#endif

     AudioSystem::setParameters(0, (String8)"AudioSetMatvDigitalEnable=1");
    // tell Audio Driver we are going to start: for Daul Speaker (ClassD + ClassAB)
    if (mFd >= 0 ) {
       int Reg_Data[3] = {INFO_U2K_MATV_AUDIO_START, 0, 0};
       ::ioctl(mFd,YUSU_INFO_FROM_USER,&Reg_Data);
    }

    //set MATV
    /*
    if ( !I2SSet(mI2Sdriver,MATV) ){
        SXLOGE("I2S driver set MATV fail\n");
        mState = STATE_ERROR;
        return ERROR_OPEN_FAILED;
    }
    */

#ifdef MATV_AUDIO_TEST_I2S
    if ( !tvscan_finish )  {                      //start the chip and scan channel if the scan is not done yet
        SXLOGD("Force start matv chip");
        startMATVChip();
    }
#endif

    // Start I2S driver
    if(!mI2SStartFlag) {
       if(!I2SStart(mI2Sdriver,mI2Sid,MATV)){
           SXLOGE("I2S start fialed");
           reset_nosync();
           return ERROR_START_FAILED;
       }
       mI2SStartFlag = 1;
    }

    mPaused = false;
    mRender = true;

    // wake up render thread
    SXLOGD("start wakeup render thread\n");
    mCondition.signal();

    return NO_ERROR;
}

status_t mATVAudioPlayer::stop()
{
    SXLOGD("[%d]stop\n",mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        SXLOGE("stop ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

    // Stop MATV chip audio
    spATVCtrlService->ATVCS_matv_audio_stop();
    //matv_audio_stop();

	  if (matv_use_analog_input == 1)
	  {
	    SXLOGD("line in path => stop");
	    // tell Audio Driver we are going to stop: for Daul Speaker (ClassD + ClassAB)
            if (mFd >= 0 ) {
               int Reg_Data[3] = {INFO_U2K_MATV_AUDIO_STOP, 0, 0};
	       ::ioctl(mFd,YUSU_INFO_FROM_USER,&Reg_Data);
	    }
	    AudioSystem::setParameters(0, (String8)"AtvAudioLineInEnable=0");
	    return NO_ERROR;
	  }

    AudioSystem::setParameters(0, (String8)"AudioSetMatvDigitalEnable=0");
    // stop I2S driver
    if(mI2SStartFlag == 1){                    //only when I2S is open
       if(!I2SStop(mI2Sdriver,mI2Sid,MATV)){
           SXLOGE("I2S stop fialed");
           reset_nosync();
           return ERROR_STOP_FAILED;
       }
       mI2SStartFlag = 0;
    }

    mPaused = true;
    mRender = false;
    return NO_ERROR;
}

status_t mATVAudioPlayer::seekTo(int position)
{
    SXLOGD("[%d]seekTo %d\n", position, mI2Sid);
    Mutex::Autolock l(mMutex);

    return NO_ERROR;
}

status_t mATVAudioPlayer::pause()
{
    SXLOGD("[%d]pause\n",mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        SXLOGD("pause ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

    if (matv_use_analog_input ==1 )
    {
      SXLOGD("line in path => pause means stop");
	    // tell Audio Driver we are going to stop: for Daul Speaker (ClassD + ClassAB)
	    if (mFd >= 0 ) {
            int Reg_Data[3] = {INFO_U2K_MATV_AUDIO_STOP, 0, 0};
	    ::ioctl(mFd,YUSU_INFO_FROM_USER,&Reg_Data);
	    }
	    ///AudioSystem::setParameters(0, (String8)"AtvAudioLineInEnable=0");
	    AudioSystem::setStreamMute(AUDIO_STREAM_MATV, true);
	    return NO_ERROR;
     }

    //I2S uses mute to pause
#ifdef MUTE_PAUSE
    if ( mMutePause == false) {
       //mAudioSink->mute(1);
       mMutePause = true;
    }
#else
    mPaused = true;
#endif
    return NO_ERROR;
}

bool mATVAudioPlayer::isPlaying()
{
    SXLOGD("[%d]isPlaying\n",mI2Sid);
    if (mState == STATE_OPEN) {
        return mRender;
    }
    return false;
}

status_t mATVAudioPlayer::getCurrentPosition(int* position)
{
    SXLOGD("[%d]getCurrentPosition always return 0\n", mI2Sid);
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        SXLOGD("getCurrentPosition(): file not open");
        return ERROR_NOT_OPEN;
    }
    *position = 0;
    return NO_ERROR;
}

status_t mATVAudioPlayer::getDuration(int* duration)
{
    Mutex::Autolock l(mMutex);
    if (mState != STATE_OPEN) {
        SXLOGD("getDuration ERROR_NOT_OPEN \n");
        return ERROR_NOT_OPEN;
    }

    *duration = 1000;
    SXLOGD("[%d]getDuration duration, always return 0 \n",mI2Sid);
    return NO_ERROR;
}

status_t mATVAudioPlayer::release()
{
    SXLOGD("[%d]release\n",mI2Sid);
    Mutex::Autolock l(mMutex);
    reset_nosync();

    if (matv_use_analog_input == 1)
    {
      SXLOGD("line in path => Not Stopping the render thread");
      return NO_ERROR;
    }

    // TODO: timeout when thread won't exit
    // wait for render thread to exit
    if (mRenderTid > 0) {
        mExit = true;
        SXLOGD("release signal \n");
        mCondition.signal();
        SXLOGD("release wait \n");
        mCondition.wait(mMutex);
    }
    return NO_ERROR;
}

status_t mATVAudioPlayer::reset()
{
    SXLOGD("[%d]reset\n", mI2Sid);
    Mutex::Autolock l(mMutex);
    return reset_nosync();
}

// always call with lock held
status_t mATVAudioPlayer::reset_nosync()
{
    SXLOGD("[%d]reset_nosync start\n",mI2Sid);

    // Stop MATV chip audio
    //matv_audio_stop();
    spATVCtrlService->ATVCS_matv_audio_stop();

    // reset flags
    mState = STATE_ERROR;
    mPlayTime = -1;
    mDuration = -1;
    mPaused = false;
    mRender = false;

    if (matv_use_analog_input == 1)
	  {
	    SXLOGD("line in path => stop");
	    if (mFd >= 0 ) {
              int Reg_Data[3] = {INFO_U2K_MATV_AUDIO_STOP, 0, 0};
	       ::ioctl(mFd,YUSU_INFO_FROM_USER,&Reg_Data);
	    }
	    AudioSystem::setParameters(0, (String8)"AtvAudioLineInEnable=0");
	    return NO_ERROR;
    }

    AudioSystem::setParameters(0, (String8)"AudioSetMatvDigitalEnable=0");
    // close I2S driver
    if (mI2Sdriver && mI2Sid!=0) {
        I2SStop(mI2Sdriver,mI2Sid,(I2STYPE)MATV);
        I2SClose(mI2Sdriver,mI2Sid);
    }
    mI2Sid = 0;
    mI2SStartFlag = 0;

    SXLOGD("[%d]reset_nosync end\n",mI2Sid);
    return NO_ERROR;
}

status_t mATVAudioPlayer::setLooping(int loop)
{
    SXLOGD("[%d]setLooping, do nothing \n",mI2Sid);
    return NO_ERROR;
}

#define MATV_AUDIO_SAMPLING_RATE    32000
#define MATV_AUDIO_CHANNEL_NUM      2

status_t mATVAudioPlayer::createOutputTrack() {
    // open audio track

    SXLOGD("[%d]Create AudioTrack object: rate=%d, channels=%d\n",mI2Sid, MATV_AUDIO_SAMPLING_RATE, MATV_AUDIO_CHANNEL_NUM);

    if (mAudioSink->open(MATV_AUDIO_SAMPLING_RATE, MATV_AUDIO_CHANNEL_NUM, AUDIO_FORMAT_PCM_16_BIT, 3) != NO_ERROR) {
        SXLOGE("mAudioSink open failed");
        return ERROR_OPEN_FAILED;
    }
    return NO_ERROR;
}

int mATVAudioPlayer::renderThread(void* p) {
    return ((mATVAudioPlayer*)p)->render();
}

//#define AUDIOBUFFER_SIZE 4096


int mATVAudioPlayer::render() {
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
    int drop_buffer_number = 0;

    extern int using_class_ab_amp;	//from libaudiosetting.so, related to USING_CLASS_AB_AMP
    if (using_class_ab_amp == 1) {
        SXLOGD("using class AB for mATVAudioPlayer");
        drop_buffer_number = 3;
    }
    else {
        SXLOGD("using class D for mATVAudioPlayer");
        drop_buffer_number = 1;
    }

#ifdef ATV_AUDIO_FILELOG
   FILE *fp;
   fp = fopen("sdcard/test.pcm","wb");
   SXLOGD("fp:%d", fp);
#endif

    // Open I2S driver here
    /*
    mI2Sdriver = I2SGetInstance();
    if ( mI2Sdriver == NULL ){
        SXLOGE("I2S driver doesn't exists\n");
        mAudioBuffer = new char[10];			//fake buffer for delete
        goto threadExit;
    }
    */

    // allocate render buffer
    //mAudioBuffer = new char[AUDIOBUFFER_SIZE];

    bufSize = I2SGetReadBufferSize(mI2Sdriver);
    SXLOGD("got buffer size = %d", bufSize);
    mAudioBuffer = new char[bufSize*2];
    mDummyBuffer = new char[bufSize*2];
    memset(mDummyBuffer, 0, bufSize);


    SXLOGD("mAudioBuffer: 0x%p \n",mAudioBuffer);
    if (!mAudioBuffer) {
        SXLOGD("mAudioBuffer allocate failed\n");
        goto threadExit;
    }


    // if set prority false , force to set priority
    if(t_result == -1)
    {
       struct sched_param sched_p;
       sched_getparam(0, &sched_p);
       sched_p.sched_priority = RTPM_PRIO_MATV_AUDIOPLAYER;
       if(0 != sched_setscheduler(0, SCHED_RR, &sched_p))
       {
          SXLOGE("[%s] failed, errno: %d", __func__, errno);
       }
       else
       {
          sched_p.sched_priority = RTPM_PRIO_MATV_AUDIOPLAYER;
          sched_getparam(0, &sched_p);
          SXLOGD("sched_setscheduler ok, priority: %d", sched_p.sched_priority);
       }
    }

    // let main thread know we're ready
    {
        Mutex::Autolock l(mMutex);
        mRenderTid = myTid();
        SXLOGD("[%d]render start mRenderTid=%d\n",mI2Sid, mRenderTid);
        mCondition.signal();
    }

    while (1) {
        long numread = 0;
        {
            Mutex::Autolock l(mMutex);

            // pausing?
            if (mPaused) {
                SXLOGD("render - pause\n");
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
                SXLOGD("render - signal wait\n");
                mCondition.wait(mMutex);
                frameCount = 0;
                SXLOGD("render - signal rx'd\n");
            }
            if (mExit) break;

            // We could end up here if start() is called, and before we get a
            // chance to run, the app calls stop() or reset(). Re-check render
            // flag so we don't try to render in stop or reset state.
            if (!mRender) continue;

            if (!mAudioSink->ready()) {
                 SXLOGD("render - create output track\n");
                 if (createOutputTrack() != NO_ERROR)
                     break;
            }

            // get data from I2S driver
            /*
            int sineTableSize = 320*sizeof(uint16_t);
            char * ptr = mAudioBuffer;
            memcpy(ptr, sineTable, sineTableSize);
            ptr += sineTableSize;
            memcpy(ptr, sineTable, sineTableSize);
            ptr += sineTableSize;

            numread = sineTableSize * 2;
            */
            //numread = I2SRead(mI2Sdriver, mI2Sid, mAudioBuffer, bufSize);
            //SXLOGD("[%d]read %d bytes from I2S, id:%d",&mI2Sid, numread, mI2Sid);

        }

        // codec returns negative number on error
        if (numread < 0) {
            SXLOGE("Error in Vorbis decoder numread=%ld",numread);
            sendEvent(MEDIA_ERROR);
            break;
        }

        // create audio output track if necessary
        /*
        if (!mAudioSink->ready()) {
            SXLOGD("render - create output track\n");
            if (createOutputTrack() != NO_ERROR)
                break;
        }*/

        // start audio output if necessary
        if (!audioStarted && !mPaused && !mExit) {
            SXLOGD("render - starting audio\n");
            mAudioSink->start();
            audioStarted = true;
            firstOutput = true;

            //firstly push some amount of buffer to make the mixer alive
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               SXLOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               SXLOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               SXLOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }
            if ((temp = mAudioSink->write(mDummyBuffer, bufSize)) < 0) {
               SXLOGE("Error in writing:%d",temp);
               result = temp;
               break;
            }

        }

        {
            Mutex::Autolock l(mMutex);
            //int brt,art;
            //SXLOGD("[%lld] before read %d",brt=getTimeMs());
            if (firstOutput) {
               numread = I2SRead(mI2Sdriver, mI2Sid, mAudioBuffer, bufSize);
               firstOutput = false;
            }
            else {
               numread = I2SRead(mI2Sdriver, mI2Sid, mAudioBuffer, bufSize);
            }
            //SXLOGD("[%lld] after read %d",art=getTimeMs());
            //if(art-brt > 90 )
             //  SXLOGW("read time abnormal");
            //SXLOGD("[%d]read %d bytes from I2S, id:%d",&mI2Sid, numread, mI2Sid);

            frameCount++;
        }

        lastTime = thisTime;
        thisTime = getTimeMs();
        if ( thisTime -lastTime > 160 )
           SXLOGW(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!time diff = %d", thisTime -lastTime);

        // Write data to the audio hardware
        dataCount+=numread;
        //SXLOGD("[%lld] mATVAudioPlayer read data count: %d",getTimeMs(), dataCount);
        /*
        if ((temp = mAudioSink->write(mAudioBuffer, numread)) < 0) {
               SXLOGD("Error in writing:%d",temp);
               result = temp;
               break;
        }
        fwrite(mAudioBuffer, 1, numread, fp);
        */

        // skip 3 buffer here to compensate the wait for class-AB
        // for class-D, we skip 1 buffer only
        if ( frameCount > drop_buffer_number ) {
           //SXLOGD("[%lld]Write data", getTimeMs());

#ifdef MUTE_PAUSE
           if ( mMutePause == 1 )
               memset(mAudioBuffer, 0, numread);
#endif

           //fwrite(mAudioBuffer, 1, numread, fp);
           if ((temp = mAudioSink->write(mAudioBuffer, numread)) < 0) {
               SXLOGE("Error in writing:%d",temp);
               result = temp;
               break;
           }
        }

        //SXLOGD("[%lld] after write",getTimeMs());

        //sleep to allow command to get mutex
        usleep(1000);
    }

threadExit:


    // tell Audio Driver we are going to stop: for Daul Speaker (ClassD + ClassAB)
    if (mFd >= 0 ) {
       int Reg_Data[3] = {INFO_U2K_MATV_AUDIO_STOP, 0, 0};
       ::ioctl(mFd,YUSU_INFO_FROM_USER,&Reg_Data);
    }

    mAudioSink.clear();


    if (mAudioBuffer) {
        delete [] mAudioBuffer;
        mAudioBuffer = NULL;
    }
    if (mDummyBuffer) {
        delete [] mDummyBuffer;
        mDummyBuffer = NULL;
    }

    SXLOGD("[%d]render end mRenderTid=%d\n",mI2Sid, mRenderTid);

    // tell main thread goodbye
    Mutex::Autolock l(mMutex);
    mRenderTid = -1;
    mCondition.signal();

#ifdef ATV_AUDIO_FILELOG
   fclose(fp);
#endif
    return result;
}



} // end namespace android

#endif //..MTK_MATV_SUPPORT
