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
#define LOG_TAG "FfmpegAudioDecoder"
#include <utils/Log.h>
#include <stdlib.h>

#include <OMX_Component.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>

#include "include/FfmpegAudioDecoder.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mem.h"
}

namespace android {

#define mp3sync(header)              ((header >> 21) & 0x7ff)
#define mp3version(header)           ((header >> 19) & 0x3)
#define mp3layer(header)             ((header >> 17) & 0x3)
#define mp3bitrate(header)           ((header >> 12) & 0xf)
#define mp3samplefreq(header)        ((header >> 10) & 0x3)
#define mp3padding(header)           ((header >> 9) & 0x1)

static int mp3_bitrate_table[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,0} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0} }
};
static int mp3_samplefreq_table[3][3] = {
	{44100, 22050, 11025},
	{48000, 24000, 12000},
	{32000, 16000, 8000}
};
static int mp3_samplenum_table[3][3] = {
	{384, 384, 384},
	{1152, 1152, 1152},
	{1152, 576, 576}
};

static void checkMpegAudioStream(uint8_t *stream, int32_t size, int32_t *offset, int32_t *framesize)
{
    uint32_t header;
    int32_t i, ver, verindex, layer, bitrate, samplefreq, padding;

    *framesize = 0;
    header = 0;
    i = 0;
    while (i < size)
    {
        header = (header << 8) | stream[i++];

        /* check validate */
        if (mp3sync(header) == 0x7ff)
        {
            ver = mp3version(header);
            layer = mp3layer(header);
            bitrate = mp3bitrate(header);
            samplefreq = mp3samplefreq(header);
            padding = mp3padding(header);

            if ((ver != 1)
                && (layer != 0)
                && (bitrate != 0xf) && (bitrate != 0x0)
                && (samplefreq != 3))
            {
                /* valid sync-word */

                bitrate = mp3_bitrate_table[1-(ver&1)][3-layer][bitrate];
                bitrate *= 1000;
                verindex = 0;
                if (ver == 0)
                {
                    verindex = 2;
                }
                else if (ver == 2)
                {
                    verindex = 1;
                }
                samplefreq = mp3_samplefreq_table[samplefreq][verindex];

                *framesize = ((mp3_samplenum_table[3-layer][verindex] >> 3) * bitrate) / samplefreq;
                if (layer == 3)
                {
                    *framesize += padding * 4;
                }
                else
                {
                    *framesize += padding;
                }

                i -= 4;
                //LOGI ("mp3 frame size = %d, offset = %d, total = %d ... ", *framesize, i, size);

                break;
            }
        }
    }

    *offset = i;
}


FfmpegAudioDecoder::FfmpegAudioDecoder(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mInputBuffer(NULL),
      mNumSamplesOutput(0),
      mTargetTimeUs(-1),
      pCodec (NULL),
      pCodecCtx (NULL), 
      mLastTimeUs (-1),
      mRsc (NULL),
      mResampleBuffer (NULL),
      mOutputFrame (NULL)
{
    mFormat = new MetaData;

    sp<MetaData> meta = mSource->getFormat();
    const char *mime = NULL;
    meta->findCString(kKeyMIMEType, &mime);
        
    int32_t sampleRate;
    CHECK(meta->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(meta->findInt32(kKeySampleRate, &sampleRate));

    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);
    mFormat->setInt32(kKeyChannelCount, mNumChannels);
    mFormat->setInt32(kKeySampleRate, sampleRate);

    int64_t durationUs;
    if (meta->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }

    mFormat->setCString(kKeyDecoderComponent, "FfmpegAudioDecoder");
}

FfmpegAudioDecoder::~FfmpegAudioDecoder() {
    if (mStarted) {
        stop();
    }

    if (mRsc != NULL) {
        audio_resample_close (mRsc);
        mRsc = NULL;
    }
    if (mResampleBuffer != NULL) {
        free (mResampleBuffer);
        mResampleBuffer = NULL;
    }
}

status_t FfmpegAudioDecoder::start(MetaData *) {
    CHECK(!mStarted);

    const char *mime = NULL;
    sp<MetaData> meta = mSource->getFormat();

    meta->findPointer(kKeyPlatformPrivate, (void **)&pCodecCtx);
    CHECK (pCodecCtx != NULL);

    LOGI ("CODEC ID = 0x%x ... ", pCodecCtx->codec_id);
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    CHECK (pCodec != NULL);
    CHECK (avcodec_open (pCodecCtx, pCodec) >= 0);

    LOGI ("sample_fmt = %d, sample_rate = %d, channels = %d ... ", pCodecCtx->sample_fmt, pCodecCtx->sample_rate, pCodecCtx->channels);

    mSource->start();
    mNumSamplesOutput = 0;
    mTargetTimeUs = -1;
    mStarted = true;

    return OK;
}

status_t FfmpegAudioDecoder::stop() {
    LOGI("%s ... ", __func__);

    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    if (mOutputFrame) {
        mOutputFrame->release();
        mOutputFrame = NULL;
    }

    avcodec_close(pCodecCtx);

    mSource->stop();
    mStarted = false;

    return OK;
}

sp<MetaData> FfmpegAudioDecoder::getFormat() {
    LOGI("%s ... ", __func__);

    return mFormat;
}

status_t FfmpegAudioDecoder::decode (MediaBuffer **out) {

    if (mOutputFrame == NULL) {
        int32_t frameSize = 192000 * 2;

        mOutputFrame = new MediaBuffer (frameSize);
    }
    CHECK(mOutputFrame != NULL);

    uint8_t *bitstream = (uint8_t *) mInputBuffer->data() + mInputBuffer->range_offset();
    int32_t bufferSize = mInputBuffer->range_length();
    int32_t frameFinished = 192000 * 2;
    int16_t *output;

    switch (pCodecCtx->codec_id)
    {
        case CODEC_ID_MP2:
        case CODEC_ID_MP3:
        {
            int32_t offset, framesize;
            checkMpegAudioStream (bitstream, bufferSize, &offset, &framesize);
            mInputBuffer->set_range (mInputBuffer->range_offset() + offset, mInputBuffer->range_length() - offset);
            //LOGI("mpeg audio buffersize = %d, offset = %d, framesize = %d ... ", bufferSize, offset, framesize);
            bitstream = (uint8_t *) mInputBuffer->data() + mInputBuffer->range_offset();
            bufferSize = mInputBuffer->range_length();
            break;
        }

        default:
            break;
    }

    int64_t timeUs = 0;
    CHECK(mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
    if (timeUs != -1) {
        if (timeUs > mLastTimeUs) {
            mLastTimeUs = timeUs;
        }
        mInputBuffer->meta_data()->setInt64(kKeyTime, -1);
    }

    if ((pCodecCtx->sample_fmt != SAMPLE_FMT_S16) || (pCodecCtx->channels > 2))
    {
        frameFinished = 192000 * pCodecCtx->channels;
        if (mResampleBuffer == NULL) {
            mResampleBuffer = (int16_t *) malloc (frameFinished);
        }
        output = mResampleBuffer;
        CHECK (output != NULL);
    }
    else
    {
        output = (int16_t *)mOutputFrame->data();
    }

    AVPacket avpkt;
    av_init_packet (&avpkt);
    avpkt.data = bitstream;
    avpkt.size = bufferSize;

    int32_t decodesize = avcodec_decode_audio3(pCodecCtx, output, &frameFinished, &avpkt);
    //LOGI("decodesize = %d,  frameFinished = %d ... ", decodesize, frameFinished);
    if ((decodesize > 0) && (decodesize < bufferSize))
    {
        mInputBuffer->set_range (mInputBuffer->range_offset() + decodesize, bufferSize - decodesize);
    }
    else
    {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }
    if (frameFinished) {
        int32_t numBytesPerSample = av_get_bits_per_sample_format(pCodecCtx->sample_fmt) * pCodecCtx->channels / 8;
        int32_t sampleNum = frameFinished / numBytesPerSample;

        mNumSamplesOutput += sampleNum;
        mLastTimeUs += sampleNum * 1000000 / pCodecCtx->sample_rate;

        if (((pCodecCtx->sample_fmt != SAMPLE_FMT_S16) || (pCodecCtx->channels > 2)) && (mRsc == NULL)) {
            LOGI("resample [sample_fmt, channels] from [%d, %d]  to [%d, %d] ... ", pCodecCtx->sample_fmt, pCodecCtx->channels, SAMPLE_FMT_S16, mNumChannels);
            mRsc = av_audio_resample_init (mNumChannels, pCodecCtx->channels, pCodecCtx->sample_rate,
                                                                                      pCodecCtx->sample_rate, SAMPLE_FMT_S16, pCodecCtx->sample_fmt,
                                                                                      16, 10, 0, 0.8);
            CHECK(mRsc != NULL);
        }

        if (mRsc != NULL) {
            int32_t newsampleNum = audio_resample(mRsc, (short *)mOutputFrame->data(), (short *)output, sampleNum);
            CHECK (newsampleNum > 0);
            numBytesPerSample = av_get_bits_per_sample_format(SAMPLE_FMT_S16) * mNumChannels / 8;
            frameFinished = numBytesPerSample * newsampleNum;
        }

        mOutputFrame->set_range (0, frameFinished);
        mOutputFrame->meta_data()->setInt64(kKeyTime, mLastTimeUs);

        *out = mOutputFrame;
        mOutputFrame = NULL;
    }
    else
    {
        MediaBuffer *tmp = new MediaBuffer(0);
        tmp->meta_data()->setInt64(kKeyTime, mLastTimeUs);
        *out = tmp;
    }

    return OK;
}


status_t FfmpegAudioDecoder::read (MediaBuffer **out, const ReadOptions *options) {

    *out = NULL;

    bool seeking = false;
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        seeking = true;
        mNumSamplesOutput = 0;
        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }
        avcodec_flush_buffers(pCodecCtx);
    }

    if (mInputBuffer == NULL) {
        status_t err = mSource->read(&mInputBuffer, options);
        if (err != OK) {
            return err;
        }
    }

    if (seeking) {
        int64_t targetTimeUs;
        if (mInputBuffer->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs)
                && targetTimeUs >= 0) {
            mTargetTimeUs = targetTimeUs;
        } else {
            mTargetTimeUs = -1;
        }

        do {
        if (mInputBuffer->meta_data()->findInt64(kKeyTime, &targetTimeUs)) {
                mLastTimeUs = targetTimeUs;
    	 }
            if (mLastTimeUs >= mTargetTimeUs) {
                break;
            }
            mInputBuffer->release();
            status_t err = mSource->read(&mInputBuffer, NULL);
            if (err != OK) {
                return err;
            }
        } while (1);
    }

    return decode(out);
}

void FfmpegAudioDecoder::signalBufferReturned(MediaBuffer *buffer) {
    LOGI("signalBufferReturned");
}

}  // namespace android
#endif
