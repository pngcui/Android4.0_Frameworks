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

#ifndef FFMPEG_AUDIO_DECODER_H_
#define FFMPEG_AUDIO_DECODER_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>

extern "C"
{
#include "libavformat/avformat.h"
}

namespace android {

struct FfmpegAudioDecoder : public MediaSource,
                        public MediaBufferObserver {
    FfmpegAudioDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);
    virtual void signalBufferReturned(MediaBuffer *buffer);

protected:
    virtual ~FfmpegAudioDecoder();

private:

    status_t decode (MediaBuffer **out) ;

    sp<MediaSource> mSource;
    bool mStarted;

    sp<MetaData> mFormat;
    MediaBuffer *mInputBuffer;

    ReSampleContext *mRsc;
    int16_t *mResampleBuffer;

    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;
    MediaBuffer *mOutputFrame;

    int64_t mLastTimeUs;

    int32_t mNumChannels;
    int64_t mNumSamplesOutput;
    int64_t mTargetTimeUs;

    FfmpegAudioDecoder(const FfmpegAudioDecoder &);
    FfmpegAudioDecoder &operator=(const FfmpegAudioDecoder &);
};

}  // namespace android

#endif
