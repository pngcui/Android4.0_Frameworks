/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef FFMPEG_VIDEO_DECODER_H_
#define FFMPEG_VIDEO_DECODER_H_

#include <android/native_window.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/TimeSource.h>
#include <utils/threads.h>
#include <utils/Vector.h>
#include <utils/List.h>
#include <pthread.h>

extern "C"
{
#include "libavformat/avformat.h"
}

namespace android {

struct FfmpegVideoDecoder : public MediaSource,
                        public MediaBufferObserver {
    FfmpegVideoDecoder(const sp<MediaSource> &source, const sp<ANativeWindow> &nativeWindow);

    virtual status_t start(MetaData *params);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);
    virtual void signalBufferReturned(MediaBuffer *buffer);

protected:
    virtual ~FfmpegVideoDecoder();

private:

    status_t initMediaBuffer () ;
    status_t decode () ;
    sp<ANativeWindow> mNativeWindow;

    sp<MediaSource> mSource;
    bool mStarted;
    int32_t mBufWidth, mBufHeight;
    int32_t mImgWidth, mImgHeight;

    sp<MetaData> mFormat;
    MediaBuffer *mInputBuffer;

    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;
    AVFrame *pFrame;

    int64_t mLastTimeUs;
    int64_t mTargetTimeUs;

    struct OutBufferInfo {
        size_t mSize;
        void *mData;
        MediaBuffer *mMediaBuffer;
    };
    Vector<OutBufferInfo> mOutBuffers;

    List<size_t> mFilledBuffers;
    Condition mBufferFilled;

    List<size_t> mFreeBuffers;
    Condition mBufferFreed;

    Mutex mLock;

    int64_t mSeekTimeUs;
    ReadOptions::SeekMode mSeekMode;

    status_t mFinalStatus;

    static void *ThreadWrapper(void *me);
    pthread_t mThread;

    SystemTimeSource mSystemTimeSource;
    int64_t mDecFrameNum;
    int64_t mDecSumInterval;
    int64_t mDecMaxInterval;
    int64_t mDecMinInterval;

    int64_t mCpyFrameNum;
    int64_t mCpySumInterval;
    int64_t mCpyMaxInterval;
    int64_t mCpyMinInterval;

    FfmpegVideoDecoder(const FfmpegVideoDecoder &);
    FfmpegVideoDecoder &operator=(const FfmpegVideoDecoder &);
};

}  // namespace android

#endif
