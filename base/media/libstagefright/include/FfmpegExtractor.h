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

#ifndef FFMPEGEXTRACTOR_H_

#define FFMPEGEXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <utils/Vector.h>

extern "C"
{
#include "libavformat/avformat.h"
}

namespace android {

struct AMessage;
class DataSource;
class String8;
struct FfmpegExtractorTrack;

class FfmpegExtractor : public MediaExtractor {
public:
    // Extractor assumes ownership of "source".
    FfmpegExtractor(const sp<DataSource> &source);

    virtual size_t countTracks();
    virtual sp<MediaSource> getTrack(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags);

    virtual sp<MetaData> getMetaData();

    status_t read();
    void seekTo(int64_t seekTimeUs);

    int IORead( void *opaque, uint8_t *buf, int buf_size );
    int64_t IOSeek( void *opaque, int64_t offset, int whence );


protected:
    virtual ~FfmpegExtractor();

private:
    Mutex mLock;

    sp<DataSource> mDataSource;
    sp<MetaData> mFileMeta;
    bool mInit;
    int64_t mStartTime;
    off_t mOffset;

    int32_t mSeekCount;

    Vector< sp<FfmpegExtractorTrack> >  mTracks;
    unsigned int mTrackCount;
    AVFormatContext *pAvformatCtx;
    URLContext *mUrl;
    uint8_t* mIoBuffer;

    FfmpegExtractor(const FfmpegExtractor &);
    FfmpegExtractor &operator=(const FfmpegExtractor &);
};

bool SniffFfmpeg(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);

}  // namespace android

#endif  // FFMPEGEXTRACTOR_H_