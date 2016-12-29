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

#if defined(USE_FFMPEG)
//#define LOG_NDEBUG 0
#define LOG_TAG "FfmpegExtractor"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <utils/String8.h>

#include "include/FfmpegExtractor.h"

extern "C"
{
#include "libavformat/avformat.h"
}

#define INT64_MIN       (-0x7fffffffffffffffLL - 1)
#define INT64_MAX      INT64_C(9223372036854775807)

#define PRE_READ_FRAME_NUM 30

namespace android {

static const struct
{
    const char  *mimestr;
    enum CodecID ffmpeg_codec_id;
} codecs_table[] =
{
    /*
     * Video Codecs
     */
    {MEDIA_MIMETYPE_VIDEO_AVC, CODEC_ID_H264},
    {MEDIA_MIMETYPE_VIDEO_MPEG4, CODEC_ID_MPEG4},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MPEG1VIDEO},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MPEG2VIDEO},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MPEG2VIDEO_XVMC},
    {MEDIA_MIMETYPE_VIDEO_H263, CODEC_ID_H263},
    {MEDIA_MIMETYPE_VIDEO_H263, CODEC_ID_H263I},
    {MEDIA_MIMETYPE_VIDEO_H263, CODEC_ID_H263P},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_VP3},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_VP5},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_VP6},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_VP6F},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_VP6A},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_VP8},
    {MEDIA_MIMETYPE_VIDEO_VC1, CODEC_ID_VC1},
    {MEDIA_MIMETYPE_VIDEO_VC1, CODEC_ID_WMV3},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_RV10},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_RV20},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_RV30},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_RV40},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MJPEG},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_WMV1},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_WMV2},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MSMPEG4V1},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MSMPEG4V2},
    {MEDIA_MIMETYPE_VIDEO_FFMPEG, CODEC_ID_MSMPEG4V3},

    /*
     *  Audio Codecs
     */
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_AMR_NB},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_AMR_WB},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_MP2},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_MP3},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_MP3ADU},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_MP3ON4},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_WMALOSSLESS},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_WMAPRO},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_WMAV1},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_WMAV2},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_WMAVOICE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_AAC},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_AC3},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_QCELP},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_FLAC},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_VORBIS},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_COOK},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ATRAC3},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_RA_144},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_RA_288},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S16LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S16BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U16LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U16BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S8},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U8},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_MULAW},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_ALAW},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S32LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S32BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U32LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U32BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S24LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S24BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U24LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_U24BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S24DAUD},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_ZORK},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_S16LE_PLANAR},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_DVD},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_F32BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_F32LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_F64BE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_F64LE},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_PCM_BLURAY},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_QT},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_WAV},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_DK3},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_DK4},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_WS},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_SMJPEG},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_MS},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_4XM},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_XA},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_ADX},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_EA},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_G726},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_CT},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_SWF},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_YAMAHA},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_SBPRO_4},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_SBPRO_3},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_SBPRO_2},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_THP},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_AMV},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_EA_R1},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_EA_R3},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_EA_R2},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_EA_SEAD},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_EA_EACS},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_EA_XAS},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_EA_MAXIS_XA},
    {MEDIA_MIMETYPE_AUDIO_FFMPEG, CODEC_ID_ADPCM_IMA_ISS},

    {"", CODEC_ID_NONE},
};

const char * GetCodecMimeStr (enum CodecID ffmpeg_codec_id)
{
    for( unsigned i = 0; codecs_table[i].ffmpeg_codec_id != CODEC_ID_NONE; i++ )
    {
        if ( codecs_table[i].ffmpeg_codec_id == ffmpeg_codec_id )
        {
            return codecs_table[i].mimestr;
        }
    }
    return NULL;
}
/////////////////////////////////////////////////////////////////////////////////////////

struct FfmpegExtractorTrack : public MediaSource {
    FfmpegExtractorTrack(const sp<MetaData> &meta, int32_t trackIndex, enum CodecID id, uint8_t *extradata, uint32_t size);

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options = NULL);

    bool hasBufferAvailable(status_t *finalResult);
    void queueAccessPacket(MediaBuffer *mediabuffer);
    void signalEOS(status_t result);
    void clear();
    int32_t getTrackIndex();
    bool isMpeg2Stream();
    bool isMpeg2KeyFrame(const uint8_t *data, uint32_t size);

protected:
    virtual ~FfmpegExtractorTrack();

private:
    Mutex mLock;
    Condition mCondition;

    int32_t mTrackIndex;
    sp<MetaData> mFormat;
    List<MediaBuffer *> mPackets;
    status_t mEOSResult;

    bool mIsAVC;
    int32_t mNALLengthSize;

    bool mIsMpeg2;

    size_t parseNALSize(const uint8_t *data) const;
    void addESDSFromCodecSpecificInfo(uint8_t type, uint8_t *extradata, uint32_t size);

    FfmpegExtractorTrack(const FfmpegExtractorTrack &);
    FfmpegExtractorTrack &operator=(const FfmpegExtractorTrack &);
};

FfmpegExtractorTrack::FfmpegExtractorTrack(const sp<MetaData> &meta, int32_t trackIndex, enum CodecID id, uint8_t *extradata, uint32_t size)
    : mFormat(meta),
      mTrackIndex(trackIndex),
      mIsAVC(false),
      mIsMpeg2(false),
      mNALLengthSize(0),
      mEOSResult(OK)
{
    const char *mime = NULL;
    mFormat->findCString(kKeyMIMEType, &mime);
    if (strcmp (mime, MEDIA_MIMETYPE_AUDIO_AAC) == 0)
    {
        addESDSFromCodecSpecificInfo(0x40, extradata, size);
    }
    else if (strcmp (mime, MEDIA_MIMETYPE_VIDEO_MPEG4) == 0)
    {
        addESDSFromCodecSpecificInfo(0x2f, extradata, size);
    }
    else if (strcmp (mime, MEDIA_MIMETYPE_VIDEO_AVC) == 0)
    {
        if (extradata[0] == 1)
        {
            /* avcC */
            mIsAVC = true;
            mFormat->setData(kKeyAVCC, kKeyAVCC, extradata, size);
            CHECK(size >= 7);
            mNALLengthSize = 1 + (extradata[4] & 3);

            LOGI ("AVC stream, NAL length size = %d ... ", mNALLengthSize);
        }
        else
        {
            addESDSFromCodecSpecificInfo(0, extradata, size);
        }
    }
    else if (strcmp (mime, MEDIA_MIMETYPE_VIDEO_VC1) == 0)
    {
        struct _BitmapInfoHhr
        {
            uint32_t    BiSize;
            uint32_t    BiWidth;
            uint32_t    BiHeight;
            uint16_t    BiPlanes;
            uint16_t    BiBitCount;
            uint32_t    BiCompression;
            uint32_t    BiSizeImage;
            uint32_t    BiXPelsPerMeter;
            uint32_t    BiYPelsPerMeter;
            uint32_t    BiClrUsed;
            uint32_t    BiClrImportant;
        } BitmapInfoHhr;

        uint8_t *vc1extra = (uint8_t *)malloc(40 + size);
        CHECK(vc1extra != NULL);
        BitmapInfoHhr.BiSize = 40 + size;
        mFormat->findInt32(kKeyWidth, (int32_t *)&BitmapInfoHhr.BiWidth);
        mFormat->findInt32(kKeyHeight, (int32_t *)&BitmapInfoHhr.BiHeight);
        if (id == CODEC_ID_WMV3)
        {
            BitmapInfoHhr.BiCompression = 0x33564d57;
        }
        else
        {
            BitmapInfoHhr.BiCompression = 0x31435657;
        }
        memcpy(vc1extra, &BitmapInfoHhr, 40);
        memcpy(vc1extra+40, extradata, size);

        addESDSFromCodecSpecificInfo(0, vc1extra, 40 + size);

        free (vc1extra);
    }
    else if (strcmp (mime, MEDIA_MIMETYPE_VIDEO_MPEG2) == 0)
    {
        mIsMpeg2 = true;
        addESDSFromCodecSpecificInfo(0, extradata, size);
    }

}

FfmpegExtractorTrack::~FfmpegExtractorTrack() {
}

void FfmpegExtractorTrack::addESDSFromCodecSpecificInfo(uint8_t type, uint8_t *extradata, uint32_t size)
{
    static uint8_t kStaticESDS[] = {
        0x03, 0x80, 0,
        0x00, 0x00,     // ES_ID
        0x00,              // streamDependenceFlag, URL_Flag, OCRstreamFlag

        0x04, 0x80, 0,
        0x40,                       // Audio ISO/IEC 14496-3
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,

        0x05, 0x80, 0,
        // AudioSpecificInfo (with size prefix) follows
    };

    CHECK(size < 16384);
    size_t esdsSize = sizeof(kStaticESDS) + size;
    CHECK(esdsSize < 16384);

    kStaticESDS[1] = ((esdsSize - 3) >> 7) | 0x80;
    kStaticESDS[2] = ((esdsSize - 3) & 0x7f);

    kStaticESDS[7] = ((esdsSize - 9) >> 7) | 0x80;
    kStaticESDS[8] = ((esdsSize - 9) & 0x7f);

    kStaticESDS[9] = type;

    kStaticESDS[23] = (size >> 7) | 0x80;
    kStaticESDS[24] = (size & 0x7f);

    uint8_t *esds = new uint8_t[esdsSize];
    memcpy(esds, kStaticESDS, sizeof(kStaticESDS));
    uint8_t *ptr = esds + sizeof(kStaticESDS);
    memcpy(ptr, extradata, size);

    mFormat->setData(kKeyESDS, kKeyESDS, esds, esdsSize);

    delete[] esds;
    esds = NULL;
}

status_t FfmpegExtractorTrack::start(MetaData *params) {
    return OK;
}

status_t FfmpegExtractorTrack::stop() {
    return OK;
}

sp<MetaData> FfmpegExtractorTrack::getFormat() {
    return mFormat;
}

size_t FfmpegExtractorTrack::parseNALSize(const uint8_t *data) const {
    switch (mNALLengthSize) {
        case 1:
            return *data;
        case 2:
            return U16_AT(data);
        case 3:
            return ((size_t)data[0] << 16) | U16_AT(&data[1]);
        case 4:
            return U32_AT(data);
    }

    // This cannot happen, mNALLengthSize springs to life by adding 1 to a 2-bit integer.
    CHECK(!"Should not be here.");

    return 0;
}

status_t FfmpegExtractorTrack::read(MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    bool seek = false;
    int64_t seekTimeUs = -1;
    ReadOptions::SeekMode seekMode;
    if (options && options->getSeekTo(&seekTimeUs, &seekMode)) {
        if (seekTimeUs < 0) {
            seekTimeUs = 0;
        }
        seek = true;
    }

    Mutex::Autolock autoLock(mLock);
    while (mEOSResult == OK && mPackets.empty()) {
        mCondition.wait(mLock);
    }

    if (!mPackets.empty()) {
        MediaBuffer *mediaBuffer = *mPackets.begin();
        mPackets.erase(mPackets.begin());

        if (mIsAVC)
        {
            int32_t nalsize, totalsize = 0;
            uint8_t *bitstream = (uint8_t *) mediaBuffer->data() + mediaBuffer->range_offset();
            int32_t bufferSize = mediaBuffer->range_length();

            if (bufferSize > 0)
            {
                int32_t srcOffset = 0;
                int32_t dstOffset = 0;
                while (srcOffset < bufferSize)
                {
                    CHECK((srcOffset + mNALLengthSize) <= bufferSize);
                    nalsize = parseNALSize(bitstream + srcOffset);
                    totalsize += nalsize + 4; /* sync word: 0, 0, 0, 1 */
                    srcOffset += nalsize + mNALLengthSize;
                }

                MediaBuffer *newBuffer = new MediaBuffer(totalsize);
                CHECK(newBuffer != NULL);
                uint8_t *dststream = (uint8_t *) newBuffer->data();

                srcOffset = 0;
                while (srcOffset < bufferSize)
                {
                    nalsize = parseNALSize(bitstream + srcOffset);
                    srcOffset += mNALLengthSize;
                    CHECK((srcOffset + nalsize) <= bufferSize);

                    dststream[dstOffset++] = 0;
                    dststream[dstOffset++] = 0;
                    dststream[dstOffset++] = 0;
                    dststream[dstOffset++] = 1;
                    memcpy (dststream+dstOffset, bitstream + srcOffset, nalsize);
                    srcOffset += nalsize;
                    dstOffset += nalsize;
                }

                newBuffer->set_range(0, dstOffset);

                int64_t timeUs = 0;
                mediaBuffer->meta_data()->findInt64(kKeyTime, &timeUs);
                newBuffer->meta_data()->setInt64(kKeyTime, timeUs);

                int32_t keyframe = 0;
                mediaBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyframe);
                if (keyframe != 0) 
                {
                    newBuffer->meta_data()->setInt32(kKeyIsSyncFrame, 1);
                }

                mediaBuffer->release();
                mediaBuffer = newBuffer;
            }
        }

        if (seek)
        {
         //   int64_t timeUs;
        //    mediaBuffer->meta_data()->findInt64(kKeyTime, &timeUs);

        //    LOGI("packet pts = %lld, target time = %lld ... ", timeUs, seekTimeUs);

        //    seekTimeUs = (seekTimeUs > timeUs) ? seekTimeUs : timeUs;
            mediaBuffer->meta_data()->setInt64(kKeyTargetTime, seekTimeUs);
        }

#if 0
        {
            FILE *fp = NULL;
            char filename[128] = {0, };
            sprintf (filename, "/system/tmp/%lld.es", timeUs);
            fp = fopen(filename, "wb");
            if (fp != NULL)
            {
                fwrite (packet->data, 1, packet->size, fp);
                fclose (fp);
            }
        }
#endif

        *out = mediaBuffer;
        return OK;
    }

    return mEOSResult;
}

void FfmpegExtractorTrack::queueAccessPacket(MediaBuffer *mediaBuffer) {
    Mutex::Autolock autoLock(mLock);
    mPackets.push_back(mediaBuffer);
    mCondition.signal();
}

void FfmpegExtractorTrack::clear() {
    Mutex::Autolock autoLock(mLock);
    while (!mPackets.empty()) {
        MediaBuffer *mediaBuffer = *mPackets.begin();
        mPackets.erase(mPackets.begin());
        mediaBuffer->release();
    }
    mPackets.clear();
    mEOSResult = OK;
}

void FfmpegExtractorTrack::signalEOS(status_t result) {
    CHECK(result != OK);

    Mutex::Autolock autoLock(mLock);
    mEOSResult = result;
    mCondition.signal();
}

bool FfmpegExtractorTrack::hasBufferAvailable(status_t *finalResult) {
    Mutex::Autolock autoLock(mLock);
    if (!mPackets.empty()) {
        return true;
    }

    *finalResult = mEOSResult;
    return false;
}

int32_t FfmpegExtractorTrack::getTrackIndex() {
    return mTrackIndex;
}

bool FfmpegExtractorTrack::isMpeg2Stream() {
    return mIsMpeg2;
}

bool FfmpegExtractorTrack::isMpeg2KeyFrame(const uint8_t *data, uint32_t size) {
    bool keyframe = false;
    uint32_t sync = 0xffffffff;
    uint32_t offset = 0;

    while ((sync != 0x100) && (offset < size)) {
        sync = (sync << 8) | data[offset++];
    }

    if ((sync == 0x100) && ((offset + 2) < size)) {
        /* find pic header */
        offset++;
        uint8_t pic_type = (data[offset] >> 3) & 0x7;
        LOGI ("mpeg2 frame type %d ... ", pic_type);
        keyframe = (pic_type == 1);
    }

    return keyframe;
}

//////////////////////////////////////////////////////////////////////////////////////////////
struct FfmpegSource : public MediaSource {
    FfmpegSource(const sp<FfmpegExtractor> &extractor, const sp<FfmpegExtractorTrack> &impl);

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();

    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options = NULL);

protected:
    virtual ~FfmpegSource();

private:
    sp<FfmpegExtractor> mExtractor;
    sp<FfmpegExtractorTrack> mImpl;

    FfmpegSource(const FfmpegSource &);
    FfmpegSource &operator=(const FfmpegSource &);
};

FfmpegSource::FfmpegSource(const sp<FfmpegExtractor> &extractor, const sp<FfmpegExtractorTrack> &impl)
    : mExtractor(extractor),
      mImpl(impl)
{
}

FfmpegSource::~FfmpegSource()
{
}

status_t FfmpegSource::start(MetaData *params) {
    return mImpl->start(params);
}

status_t FfmpegSource::stop() {
    return mImpl->stop();
}

sp<MetaData> FfmpegSource::getFormat() {
    return mImpl->getFormat();
}

status_t FfmpegSource::read(MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    status_t err;
    int64_t seekTimeUs;
    ReadOptions::SeekMode seekMode;
    status_t finalResult;
    bool seek = false;

    if (options && options->getSeekTo(&seekTimeUs, &seekMode)) {
        LOGI ("track %d, seek to %lld ... ", mImpl->getTrackIndex(), seekTimeUs);
        if (seekTimeUs < 0) {
            seekTimeUs = 0;
        }
        seek = true;
        mExtractor->seekTo(seekTimeUs);
    }

readagain:

    while (!mImpl->hasBufferAvailable(&finalResult)) {
        if (finalResult != OK) {
            return ERROR_END_OF_STREAM;
        }

        err = mExtractor->read();
        if (err != OK) {
            mImpl->signalEOS(err);
        }
    }

    err = mImpl->read(out, options);
    if (mImpl->isMpeg2Stream()) {
        const uint8_t *buf = (const uint8_t *)(*out)->data();
        if ((buf[0] != 0) || (buf[1] != 0) || (buf[2] != 1)){
            LOGE("mpeg2 packet error, release mediabuffer and read again ...... ");
            (*out)->release();
            goto readagain;
        }
        if (seek && !mImpl->isMpeg2KeyFrame(buf, (*out)->range_length())) {
            LOGE("mpeg2 packet not key frame, release mediabuffer and read again ...... ");
            (*out)->release();
            goto readagain;
        }
    }

    return err;
}

//////////////////////////////////////////////////////////////////////////////////////////
extern "C" int IORead_C(void *opaque, uint8_t *buf, int buf_size);
extern "C" int64_t IOSeek_C(void *opaque, int64_t offset, int whence);

FfmpegExtractor::FfmpegExtractor(const sp<DataSource> &source)
    : mDataSource(source),
      mInit(false),
      mTrackCount(0),
      pAvformatCtx(NULL),
      mUrl(NULL),
      mIoBuffer(NULL),
      mSeekCount (0),
      mFileMeta(new MetaData),
      mOffset(0)
{
    bool haveAudio = false;
    bool haveVideo = false;
    int32_t audioIndex, videoIndex;

    audioIndex = -1;
    videoIndex = -1;

#if 1
    uint8_t *buffer = NULL;
    AVInputFormat *fmt;
    AVProbeData   pd;
    ByteIOContext*   io;
    int io_buffer_size = 32768;  /* FIXME */

    buffer = (uint8_t *)malloc(1024 * 1024);
    CHECK (buffer != NULL);

    pd.filename = NULL;
    pd.buf = buffer;
    pd.buf_size = 1024 * 1024;

    int32_t size = source->readAt(0, buffer, pd.buf_size);
    if (size <= 0)
    {
        LOGE(" datasource read error ... ");
        goto err_exit;
    }
    pd.buf_size = size;

    /* Guess format */
    if( !( fmt = av_probe_input_format( &pd, 1 ) ) )
    {
        LOGE("FfmpegExtractor: av_probe_input_format error ... ");
        goto err_exit;
    }

    /* Create I/O wrapper */
    mIoBuffer = (uint8_t*)malloc( io_buffer_size );
    if(mIoBuffer == NULL)
    {
        goto err_exit;		
    }
	
    mUrl = (URLContext*)malloc( sizeof(URLContext) );
    if(mUrl == NULL)
    {
        goto err_exit;		
    }

    mUrl->priv_data = (void*)this;
    io = av_alloc_put_byte(mIoBuffer, io_buffer_size, 0, mUrl, IORead_C, NULL, IOSeek_C);

    if (av_open_input_stream( &pAvformatCtx, io, "", fmt, NULL ))
    {
        LOGE ("av_open_input_stream error .... ");
        goto err_exit;
    }
#else
    if (av_open_input_file(&pAvformatCtx, gTestUri.string(), NULL, 0, NULL) != 0)
    {
        LOGE ("av_open_input_file [%s] error .... ", gTestUri.string());
        goto err_exit;
    }
#endif

    if (av_find_stream_info(pAvformatCtx) < 0) 
    {
        LOGE ("could not find codec parameters ... ");
        goto err_exit;
    }

    for (unsigned int i = 0; i < pAvformatCtx->nb_streams; i++ )
    {
        AVStream *s = pAvformatCtx->streams[i];
        AVCodecContext *cc = s->codec;

        LOGI ("AVCodecContext codecID = 0x%x", cc->codec_id);
        switch ( cc->codec_type )
        {
        case AVMEDIA_TYPE_AUDIO:
            if (!haveAudio)
            {
                const char *mimestr = GetCodecMimeStr(cc->codec_id);
                if (mimestr != NULL)
                {
                    LOGI ("audio stream found : %s", mimestr);
                    LOGI ("audio size = %d", cc->extradata_size);
                    if ((cc->extradata != NULL) && (cc->extradata_size > 0))
                    {
                        char extrabuffer[48] = {0,};
                        for (int j=0; j<cc->extradata_size;)
                        {
                            sprintf (extrabuffer+strlen(extrabuffer), "0x%02x ",  cc->extradata[j++]);
                            if ((j%8) == 0)
                            {
                                LOGI ("audio extradata = %s", extrabuffer);
                                memset (extrabuffer, 0, 48);
                            }
                        }
                        if ((cc->extradata_size % 8) != 0) {
                            LOGI ("audio extradata = %s", extrabuffer);
                        }
                    }
                    sp<MetaData> meta = new MetaData;
                    audioIndex = i;

                    meta->setCString(kKeyMIMEType, mimestr);
                    meta->setInt64(kKeyDuration, (s->duration * 1000000) * s->time_base.num / s->time_base.den);
                    meta->setInt32(kKeyChannelCount, cc->channels);
                    if (cc->channels > 2) {
                        meta->setInt32(kKeyChannelCount, 2);
                    }
                    meta->setInt32(kKeySampleRate, cc->sample_rate);
                    meta->setInt32(kKeyMaxInputSize, 96 * 1024);
                    haveAudio = true;

                    LOGI ("audio track channel count = %d, samplerate = %d ... ", cc->channels, cc->sample_rate);
                    LOGI ("audio stream index = %d", audioIndex);

                    meta->setPointer(kKeyPlatformPrivate,  (void *)cc);

                    sp<FfmpegExtractorTrack> pTrack = new FfmpegExtractorTrack(meta, audioIndex, cc->codec_id, cc->extradata, cc->extradata_size);
                    mTracks.push(pTrack);
                    mTrackCount++;
                }
            }
            break;

        case AVMEDIA_TYPE_VIDEO:
            if (!haveVideo)
            {
                const char *mimestr = GetCodecMimeStr(cc->codec_id);
                if (mimestr != NULL)
                {
                    LOGI ("video stream found : %s", mimestr);
                    LOGI ("video size = %d", cc->extradata_size);
                    if ((cc->extradata != NULL) && (cc->extradata_size > 0))
                    {
                        char extrabuffer[48] = {0,};
                        for (int j=0; j<cc->extradata_size;)
                        {
                            sprintf (extrabuffer+strlen(extrabuffer), "0x%02x ",  cc->extradata[j++]);
                            if ((j%8) == 0)
                            {
                                LOGI ("video extradata = %s", extrabuffer);
                                memset (extrabuffer, 0, 48);
                            }
                        }
                        if ((cc->extradata_size % 8) != 0) {
                            LOGI ("video extradata = %s", extrabuffer);
                        }
                    }
                    sp<MetaData> meta = new MetaData;
                    videoIndex = i;

                    meta->setCString(kKeyMIMEType, mimestr);
                    meta->setInt64(kKeyDuration, (s->duration * 1000000) * s->time_base.num / s->time_base.den);
                    meta->setInt32(kKeyWidth, cc->width);
                    meta->setInt32(kKeyHeight, cc->height);
                    meta->setInt32(kKeyMaxInputSize, 1024 * 1024);
                    haveVideo = true;

                    meta->setPointer(kKeyPlatformPrivate,  (void *)cc);

                    LOGI ("video stream index = %d", videoIndex);

                    sp<FfmpegExtractorTrack> pTrack = new FfmpegExtractorTrack(meta, videoIndex, cc->codec_id, cc->extradata, cc->extradata_size);
                    mTracks.push(pTrack);
                    mTrackCount++;
                }
            }
            break;

        default:
            break;
        }
    }

    if (strstr(fmt->name, "webm") != NULL) {
        mFileMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_MATROSKA);
    } else if (strstr(fmt->name, "avi") != NULL) {
        mFileMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_AVI);
    } else if (strstr(fmt->name, "rm") != NULL) {
        mFileMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_RMVB);
    } else if (strstr(fmt->name, "asf") != NULL) {
        mFileMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_ASF);
    } else if (strstr(fmt->name, "mpeg") != NULL) {
        mFileMeta->setCString(kKeyMIMEType, "video/mpeg"); //match with mediafile.java
    } else {
        mFileMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_FFMPEG);
    }

err_exit:

    free (buffer);
    if (mTrackCount != 0)
    {
        mStartTime = (pAvformatCtx->start_time != AV_NOPTS_VALUE) ? 
                             (pAvformatCtx->start_time * 1000000 / AV_TIME_BASE )  : 0;
        LOGI ("start time : %lld us ... ", mStartTime);
        mInit = true;

        if (strstr(fmt->name, "mpeg") != NULL) {
            /* bufferring some data and check the start time */
            int32_t i = 0;
            while (i < PRE_READ_FRAME_NUM) {
                read();
                i++;
            }
        }
    }
}

FfmpegExtractor::~FfmpegExtractor() {
    mTracks.clear();

    if (mUrl != NULL) {
        free(mUrl);
    }
    if (mIoBuffer != NULL) {
        free(mIoBuffer);
    }
    if (pAvformatCtx != NULL)
    {
#if 1
        av_close_input_stream(pAvformatCtx);
#else
        av_close_input_file (pAvformatCtx);
#endif
    }
}

size_t FfmpegExtractor::countTracks() {
    if (!mInit) {
        LOGE("FfmpegExtractor initialize error ... ");
        return 0;
    }

    return mTrackCount;
}

sp<MediaSource> FfmpegExtractor::getTrack(size_t index) {
    if (!mInit || index >= mTrackCount) {
        LOGI("FfmpegExtractor::getTrack parameter error");
        return NULL;
    }

    return new FfmpegSource(this, mTracks.editItemAt(index));
}

sp<MetaData> FfmpegExtractor::getTrackMetaData(size_t index, uint32_t flags)
{
    LOGI("FfmpegExtractor::getTrackMetaData ... ");
    if (!mInit || (index >= mTrackCount)) {
        return NULL;
    }

    sp<FfmpegExtractorTrack> pTrack = mTracks.editItemAt(index);

    return pTrack->getFormat();
}

sp<MetaData> FfmpegExtractor::getMetaData()
{
    LOGI("FfmpegExtractor::getMetaData ... ");
    return mFileMeta;
}

status_t FfmpegExtractor::read()
{
    AVPacket  pkt;
    sp<FfmpegExtractorTrack> pAvTrack = NULL;

    Mutex::Autolock autoLock(mLock);

    /* Read a frame */
    if ( av_read_frame(pAvformatCtx, &pkt ) )
    {
        LOGE ("av_read_frame error ... ");
        return UNKNOWN_ERROR;
    }

    for (uint32_t i=0; i<mTrackCount; i++)
    {
        if (pkt.stream_index == (mTracks.editItemAt(i))->getTrackIndex())
        {
            pAvTrack = mTracks.editItemAt(i);
            break;
        }
    }

    if (pAvTrack == NULL)
    {
        av_free_packet(&pkt);
        return OK;
    }

    //LOGI ("stream index = %d, pkt pts = %lld, dts = %lld", pAvTrack->getTrackIndex(), pkt.pts, pkt.dts);
    MediaBuffer *mediaBuffer = new MediaBuffer(pkt.size);
    AVStream *s = pAvformatCtx->streams[pAvTrack->getTrackIndex()];

    //pkt->pts = (pkt->pts == (int64_t)0x8000000000000000) ? 0 : (pkt->pts * 1000000 * s->time_base.num / s->time_base.den) - mStartTime;
    int64_t tmp = pkt.dts * 1000000 * s->time_base.num / s->time_base.den;
    pkt.dts = (pkt.dts == AV_NOPTS_VALUE) ? 0 : (tmp - mStartTime);
    //LOGI ("stream index = %d, dts = %lld, size = %d", pAvTrack->getTrackIndex(), pkt.dts, pkt.size);
    if (pkt.dts < 0)
    {
        mStartTime = tmp;
        LOGI ("dts (%lld us) < mStartTime ... ", mStartTime);
        pkt.dts = 0;
    }
    mediaBuffer->meta_data()->setInt64(kKeyTime, pkt.dts);
    memcpy(mediaBuffer->data(), pkt.data, pkt.size);
    mediaBuffer->set_range(0, pkt.size);
    if ( pkt.flags & AV_PKT_FLAG_KEY ) 
    {
        mediaBuffer->meta_data()->setInt32(kKeyIsSyncFrame, 1);
    }

    av_free_packet(&pkt);
    pAvTrack->queueAccessPacket (mediaBuffer);

    return OK;
}

void FfmpegExtractor::seekTo(int64_t seekTimeUs)
{
    LOGI ("seek to %lld ...  ", seekTimeUs);
    Mutex::Autolock autoLock(mLock);

    if (mSeekCount == 0)
    {
        int64_t seektime = (seekTimeUs + mStartTime);

        if (avformat_seek_file(pAvformatCtx, -1, INT64_MIN, seektime, INT64_MAX, 0)  < 0)
        {
            LOGE ("seek error ... ");
        } else {
            for (uint32_t i=0; i<mTrackCount; i++) {
                (mTracks.editItemAt(i))->clear();
            }
        }
    }

    mSeekCount++;
    if (mSeekCount == mTrackCount)
    {
        mSeekCount = 0;
    }
}

int FfmpegExtractor::IORead( void *opaque, uint8_t *buf, int buf_size )
{
    URLContext *p_url = (URLContext*)opaque;
    FfmpegExtractor *p_priv = (FfmpegExtractor*)p_url->priv_data;
    sp<DataSource> source = p_priv->mDataSource;
    int i_ret;
		
    if( buf_size < 0 ) return -1;
    i_ret = source->readAt(mOffset, buf, buf_size);
    mOffset += i_ret;
    return (i_ret >= 0) ? i_ret : -1;
}

extern "C" int IORead_C(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *p_url = (URLContext*)opaque;
    FfmpegExtractor *p_priv = (FfmpegExtractor*)p_url->priv_data;
    return p_priv->IORead(opaque, buf, buf_size);
}

int64_t FfmpegExtractor::IOSeek( void *opaque, int64_t offset, int whence )
{
    URLContext *p_url = (URLContext*)opaque;
    FfmpegExtractor *p_priv = (FfmpegExtractor*)p_url->priv_data;
    sp<DataSource> source = p_priv->mDataSource;
    int64_t i_absolute;
    off64_t i_size;

    source->getSize(&i_size);

    switch(whence)
    {
#ifdef AVSEEK_SIZE
        case AVSEEK_SIZE:
            return i_size;
#endif
        case SEEK_SET:
            i_absolute = (int64_t)offset;
            break;
        case SEEK_CUR:
            i_absolute = mOffset + (int64_t)offset;
            break;
        case SEEK_END:
            i_absolute = i_size + (int64_t)offset;
            break;
        default:
            return -1;

    }
	
    if( i_absolute < 0 )
    {
        LOGE("Trying to seek before the beginning");
        return -1;
    }

    if( i_size > 0 && i_absolute >= i_size )
    {
        LOGE("Trying to seek too far : EOF?");
        return -1;
    }

    mOffset = i_absolute;
    return mOffset;
}

extern "C" int64_t IOSeek_C(void *opaque, int64_t offset, int whence)
{
    URLContext *p_url = (URLContext*)opaque;
    FfmpegExtractor* p_priv = (FfmpegExtractor*)p_url->priv_data;
    return p_priv->IOSeek(opaque, offset, whence);
}

////////////////////////////////////////////////////////////////////////////////
bool SniffFfmpeg (const sp<DataSource> &source, String8 *mimeType, float *confidence, sp<AMessage> *) {
    off_t pos = 0;
    uint8_t *buffer = NULL;
    AVInputFormat *fmt;
    AVProbeData   pd;
    *confidence = 0.0f;

    buffer = (uint8_t *)malloc(1024 * 1024);
    CHECK (buffer != NULL);

    pd.filename = NULL;
    pd.buf = buffer;
    pd.buf_size = 1024 * 1024;

    int32_t size = source->readAt(0, buffer, pd.buf_size);
    if (size <= 0)
    {
        LOGE(" datasource read error ... ");
        free (buffer);
        return false;
    }
    pd.buf_size = size;

    /* Guess format */
    if( !( fmt = av_probe_input_format( &pd, 1 ) ) )
    {
        LOGE("SniffFfmpeg:  av_probe_input_format error ... ");
        free (buffer);
        return false;
    }

    LOGI(" >>>>>> SniffFfmpeg format : %s <<<<< ", fmt->name);
    *mimeType = MEDIA_MIMETYPE_CONTAINER_FFMPEG;
    if ((strstr(fmt->name, "mp4") != NULL) || (strstr(fmt->name, "ogg") != NULL))
    {
        *confidence = 0.0f;
    }
    else
    {
        *confidence = 0.8f;
    }

    free (buffer);

    return true;
}

}  // namespace android
#endif
