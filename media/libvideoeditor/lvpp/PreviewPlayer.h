/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PREVIEW_PLAYER_H_

#define PREVIEW_PLAYER_H_

#include "TimedEventQueue.h"
#include "VideoEditorAudioPlayer.h"

#include <media/MediaPlayerInterface.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/TimeSource.h>
#include <utils/threads.h>
#include "PreviewPlayerBase.h"
#include "VideoEditorPreviewController.h"
#include "NativeWindowRenderer.h"

namespace android {

struct AudioPlayerBase;
struct DataSource;
struct MediaBuffer;
struct MediaExtractor;
struct MediaSource;

struct PreviewPlayer : public PreviewPlayerBase {
    PreviewPlayer(NativeWindowRenderer* renderer);
    ~PreviewPlayer();

    //Override baseclass methods
    void reset();

    status_t play();

    status_t seekTo(int64_t timeUs);

    status_t getVideoDimensions(int32_t *width, int32_t *height) const;

    void acquireLock();
    void releaseLock();

    status_t prepare();
    status_t setDataSource(
        const char *uri, const KeyedVector<String8, String8> *headers);

    //Added methods
    status_t loadEffectsSettings(M4VSS3GPP_EffectSettings* pEffectSettings,
                                 int nEffects);
    status_t loadAudioMixSettings(M4xVSS_AudioMixingSettings* pAudioMixSettings);
    status_t setAudioMixPCMFileHandle(M4OSA_Context pAudioMixPCMFileHandle);
    status_t setAudioMixStoryBoardParam(M4OSA_UInt32 audioMixStoryBoardTS,
                            M4OSA_UInt32 currentMediaBeginCutTime,
                            M4OSA_UInt32 currentMediaVolumeVol);

    status_t setPlaybackBeginTime(uint32_t msec);
    status_t setPlaybackEndTime(uint32_t msec);
    status_t setStoryboardStartTime(uint32_t msec);
    status_t setProgressCallbackInterval(uint32_t cbInterval);
    status_t setMediaRenderingMode(M4xVSS_MediaRendering mode,
                            M4VIDEOEDITING_VideoFrameSize outputVideoSize);

    status_t resetJniCallbackTimeStamp();
    status_t setImageClipProperties(uint32_t width, uint32_t height);
    status_t readFirstVideoFrame();
    status_t getLastRenderedTimeMs(uint32_t *lastRenderedTimeMs);
    status_t setAudioPlayer(AudioPlayerBase *audioPlayer);

private:
    friend struct PreviewPlayerEvent;

    enum {
        PLAYING             = 1,
        LOOPING             = 2,
        FIRST_FRAME         = 4,
        PREPARING           = 8,
        PREPARED            = 16,
        AT_EOS              = 32,
        PREPARE_CANCELLED   = 64,
        CACHE_UNDERRUN      = 128,
        AUDIO_AT_EOS        = 256,
        VIDEO_AT_EOS        = 512,
        AUTO_LOOPING        = 1024,
        INFORMED_AV_EOS     = 2048,
    };

    void cancelPlayerEvents(bool keepBufferingGoing = false);
    status_t setDataSource_l(const sp<MediaExtractor> &extractor);
    status_t setDataSource_l(
        const char *uri, const KeyedVector<String8, String8> *headers);
    void reset_l();
    status_t play_l();
    status_t initRenderer_l();
    status_t initAudioDecoder();
    status_t initVideoDecoder(uint32_t flags = 0);
    void onVideoEvent();
    void onStreamDone();
    status_t finishSetDataSource_l();
    static bool ContinuePreparation(void *cookie);
    void onPrepareAsyncEvent();
    void finishAsyncPrepare_l();
    status_t startAudioPlayer_l();
    bool mIsChangeSourceRequired;

    NativeWindowRenderer *mNativeWindowRenderer;
    RenderInput *mVideoRenderer;

    int32_t mVideoWidth, mVideoHeight;

    //Data structures used for audio and video effects
    M4VSS3GPP_EffectSettings* mEffectsSettings;
    M4xVSS_AudioMixingSettings* mPreviewPlayerAudioMixSettings;
    M4OSA_Context mAudioMixPCMFileHandle;
    M4OSA_UInt32 mAudioMixStoryBoardTS;
    M4OSA_UInt32 mCurrentMediaBeginCutTime;
    M4OSA_UInt32 mCurrentMediaVolumeValue;
    M4OSA_UInt32 mCurrFramingEffectIndex;

    uint32_t mNumberEffects;
    uint32_t mPlayBeginTimeMsec;
    uint32_t mPlayEndTimeMsec;
    uint64_t mDecodedVideoTs; // timestamp of current decoded video frame buffer
    uint64_t mDecVideoTsStoryBoard; // timestamp of frame relative to storyboard
    uint32_t mCurrentVideoEffect;
    uint32_t mProgressCbInterval;
    uint32_t mNumberDecVideoFrames; // Counter of number of video frames decoded
    sp<TimedEventQueue::Event> mProgressCbEvent;
    bool mProgressCbEventPending;
    sp<TimedEventQueue::Event> mOverlayUpdateEvent;
    bool mOverlayUpdateEventPending;
    bool mOverlayUpdateEventPosted;

    M4xVSS_MediaRendering mRenderingMode;
    uint32_t mOutputVideoWidth;
    uint32_t mOutputVideoHeight;

    uint32_t mStoryboardStartTimeMsec;

    bool mIsVideoSourceJpg;
    bool mIsFiftiesEffectStarted;
    int64_t mImageFrameTimeUs;
    bool mStartNextPlayer;
    mutable Mutex mLockControl;

    M4VIFI_UInt8*  mFrameRGBBuffer;
    M4VIFI_UInt8*  mFrameYUVBuffer;

    void setVideoPostProcessingNode(
                    M4VSS3GPP_VideoEffectType type, M4OSA_Bool enable);
    void postProgressCallbackEvent_l();
    void onProgressCbEvent();

    void postOverlayUpdateEvent_l();
    void onUpdateOverlayEvent();

    status_t setDataSource_l_jpg();

    status_t prepare_l();
    status_t prepareAsync_l();

    void updateSizeToRender(sp<MetaData> meta);

    VideoEditorAudioPlayer  *mVeAudioPlayer;

    PreviewPlayer(const PreviewPlayer &);
    PreviewPlayer &operator=(const PreviewPlayer &);
};

}  // namespace android

#endif  // PREVIEW_PLAYER_H_

