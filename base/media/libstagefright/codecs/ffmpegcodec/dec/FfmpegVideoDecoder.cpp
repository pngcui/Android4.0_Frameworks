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
#define LOG_TAG "FfmpegVideoDecoder"
#include <utils/Log.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <OMX_Component.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>

#include "include/FfmpegVideoDecoder.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
}

#ifdef HAVE_ANDROID_OS
#include <linux/android_pmem.h>
#endif

#define FPS
#define MAX_DECODE_WAIT_TIME 3000000000LL

#ifdef FPS
int64_t beginPlayTimeUs;
#endif

namespace android {

#define MAX_SUPPORT_RESOLUTION  (1920 * 1088)
#define MAX_FRAMEBUFFER_NUMBER (2)

FfmpegVideoDecoder::FfmpegVideoDecoder(const sp<MediaSource> &source, const sp<ANativeWindow> &nativeWindow)
    : mSource(source),
      mNativeWindow(nativeWindow),
      mStarted(false),
      mInputBuffer(NULL),
      mTargetTimeUs(-1),
      mLastTimeUs (-1),
      mSeekTimeUs(-1),
      mSeekMode(ReadOptions::SEEK_CLOSEST_SYNC),
      pCodec (NULL),
      pCodecCtx (NULL), 
      pFrame (NULL),
      mDecFrameNum(0),
      mDecSumInterval(0),
      mDecMaxInterval(0),
      mDecMinInterval(30000),
      mCpyFrameNum(0),
      mCpySumInterval(0),
      mCpyMaxInterval(0),
      mCpyMinInterval(30000)
{
    mFormat = new MetaData;

    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
    mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420Planar);
    mFormat->setCString(kKeyDecoderComponent, "FfmpegVideoDecoder");
}

FfmpegVideoDecoder::~FfmpegVideoDecoder() {
    if (mStarted) {
        stop();
    }
}

status_t FfmpegVideoDecoder::initMediaBuffer () {
    status_t err = OK;

    //deinitialize the buffer
    for (size_t i = 0; i < mOutBuffers.size(); i++) {
        OutBufferInfo *info = &mOutBuffers.editItemAt(i);
        if (info->mMediaBuffer != NULL) {
            info->mMediaBuffer->setObserver(NULL);
            info->mMediaBuffer->release();
        }
    }
    mOutBuffers.clear();
    mFilledBuffers.clear();
    mFreeBuffers.clear();

    //initialize the output buffer
#ifdef FFMPEG_USER_NATIVEWINDOW_RENDER
    //get buffer from native window, use fimc1 usage for yuv buffer.
    if (mNativeWindow != NULL) {
        err = native_window_set_scaling_mode(mNativeWindow.get(),
                NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
        if (err != OK) {
            return err;
        }

#include "../../../../../../device/samsung/exynos4/include/sec_format.h"

        OMX_COLOR_FORMATTYPE eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YCbCr_420_P;
        err = native_window_set_buffers_geometry(
            mNativeWindow.get(),
            mBufWidth,
            mBufHeight,
            eColorFormat);
        if (err != OK) {
            LOGE("native_window_set_buffers_geometry failed: %s (%d)", strerror(-err), -err);
            return err;
        }

        android_native_rect_t rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = mImgWidth - 1;
        rect.bottom = mImgHeight - 1;
        err = native_window_set_crop(
            mNativeWindow.get(),
            &rect);
        if (err != OK) {
            LOGE("native_window_set_crop failed: %s (%d)", strerror(-err), -err);
            return err;
        }

        sp<MetaData> meta = mSource->getFormat();
        int32_t rotationDegrees;
        if (!meta->findInt32(kKeyRotation, &rotationDegrees)) {
            rotationDegrees = 0;
        }
        uint32_t transform;
        switch (rotationDegrees) {
            case 0: transform = 0; break;
            case 90: transform = HAL_TRANSFORM_ROT_90; break;
            case 180: transform = HAL_TRANSFORM_ROT_180; break;
            case 270: transform = HAL_TRANSFORM_ROT_270; break;
            default: transform = 0; break;
        }
        if (transform) {
            err = native_window_set_buffers_transform(
                mNativeWindow.get(), transform);
        }
        if (err != OK) {
            return err;
        }

#ifdef BOARD_USE_V4L2_ION
        err = native_window_set_usage(
                mNativeWindow.get(), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP
                                            | GRALLOC_USAGE_HW_ION | GRALLOC_USAGE_HWC_HWOVERLAY);
#else
        err = native_window_set_usage(
                mNativeWindow.get(), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP
                                            | GRALLOC_USAGE_HW_FIMC1 | GRALLOC_USAGE_HWC_HWOVERLAY);
#endif
        if (err != OK) {
            LOGE("native_window_set_usage failed: %s (%d)", strerror(-err), -err);
            return err;
        }

        int32_t minUndequeuedBufs = 0;
        err = mNativeWindow->query(mNativeWindow.get(),
                NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBufs);
        if (err != 0) {
            LOGE("NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS query failed: %s (%d)", strerror(-err), -err);
            return err;
        }

        LOGE("native_window_set_buffer_count, count = %d ", (minUndequeuedBufs+MAX_FRAMEBUFFER_NUMBER));
        err = native_window_set_buffer_count(
                mNativeWindow.get(), (minUndequeuedBufs+MAX_FRAMEBUFFER_NUMBER));
        if (err != 0) {
            LOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err), -err);
            return err;
        }

        // Dequeue buffers
        for (size_t i = 0; i < (minUndequeuedBufs+MAX_FRAMEBUFFER_NUMBER); i++) {
            ANativeWindowBuffer* buf;
            err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf);
            if (err != 0) {
                LOGE("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
                break;
            }

            sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));
            OutBufferInfo info;
            info.mData = NULL;
            info.mSize = (mBufWidth * mBufHeight * 3) >> 1;
            info.mMediaBuffer = new MediaBuffer(graphicBuffer);
            info.mMediaBuffer->setObserver(this);
            mOutBuffers.push(info);
        }

        uint32_t cancelStart;
        uint32_t cancelEnd;
        if (err != OK) {
            // If an error occurred while dequeuing we need to cancel any buffers
            // that were dequeued.
            cancelStart = 0;
            cancelEnd = mOutBuffers.size();
        } else {
            // Return the last two buffers to the native window.
            cancelStart = MAX_FRAMEBUFFER_NUMBER;
            cancelEnd = (minUndequeuedBufs+MAX_FRAMEBUFFER_NUMBER);
            for (size_t i = 0; i < cancelStart; i++) {
                mFreeBuffers.push_back(i);
            }
        }

        for (uint32_t i = cancelStart; i < cancelEnd; i++) {
            OutBufferInfo *info = &mOutBuffers.editItemAt(i);
            status_t tmp = mNativeWindow->cancelBuffer(
                    mNativeWindow.get(), info->mMediaBuffer->graphicBuffer().get());
            if (tmp != OK) {
                LOGE("cancelBuffer failed: %s (%d)", strerror(-tmp), -tmp);
            }
        }
    } 
    else
#endif
    {
        //reallocate the media buffer
        for (size_t i = 0; i < MAX_FRAMEBUFFER_NUMBER; i++) {
            OutBufferInfo info;
            info.mData = NULL;
            info.mSize = mBufWidth * mBufHeight * 3 /2;
            info.mMediaBuffer = new MediaBuffer(info.mSize);
            info.mMediaBuffer->setObserver(this);
            mOutBuffers.push(info);
            mFreeBuffers.push_back(i);
        }
    }

    return err;
}

status_t FfmpegVideoDecoder::start (MetaData *) {
    Mutex::Autolock autoLock(mLock);

    LOGI("%s ... ", __func__);
#ifdef FPS
        beginPlayTimeUs = mSystemTimeSource.getRealTimeUs();
#endif	

    CHECK(!mStarted);

    const char *mime = NULL;
    sp<MetaData> meta = mSource->getFormat();

    meta->findPointer(kKeyPlatformPrivate, (void **)&pCodecCtx);
    CHECK (pCodecCtx != NULL);

    LOGI ("CODEC ID = 0x%x ... ", pCodecCtx->codec_id);
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    CHECK (pCodec != NULL);
    CHECK (avcodec_open (pCodecCtx, pCodec) >= 0);
    pFrame = avcodec_alloc_frame();
    CHECK (pFrame != NULL);

    LOGI ("pixel format = %d, width = %d, height = %d ... ", pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
    LOGI ("time_base.den = %d, time_base.num = %d ... ", pCodecCtx->time_base.den, pCodecCtx->time_base.num);

    mImgWidth = pCodecCtx->width;
    mImgHeight = pCodecCtx->height;
    mBufWidth = ((mImgWidth+15) & ~15);
    mBufHeight = ((mImgHeight+15) & ~15);
    mFormat->setInt32(kKeyWidth, mBufWidth);
    mFormat->setInt32(kKeyHeight, mBufHeight);

    if ((mBufWidth * mBufHeight) > MAX_SUPPORT_RESOLUTION) {
        LOGE ("can't support res : %d x %d ", mImgWidth, mImgHeight);
        av_free (pFrame);
        avcodec_close(pCodecCtx);
        return ERROR_UNSUPPORTED;
    }
    mFormat->setInt32(kKeyDisplayWidth, mImgWidth);
    mFormat->setInt32(kKeyDisplayHeight, mImgHeight);
    mFormat->setRect(kKeyCropRect, 0, 0, mImgWidth - 1, mImgHeight - 1);

    status_t err = initMediaBuffer();
    if (err != OK) {
        LOGE ("initMediaBuffer error ... ");
        return err;
    }

    mSource->start();
    mTargetTimeUs = -1;

    mFinalStatus = OK;
    mStarted = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mThread, &attr, ThreadWrapper, this);
    pthread_attr_destroy(&attr);

    return OK;
}

status_t FfmpegVideoDecoder::stop() {
    LOGI("%s ... ", __func__);

    CHECK(mStarted);
    mStarted = false;
    mBufferFreed.signal();
    void *dummy;
    pthread_join(mThread, &dummy);

    Mutex::Autolock autoLock(mLock);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    for (size_t i = 0; i < mOutBuffers.size(); i++) {
        OutBufferInfo *info = &mOutBuffers.editItemAt(i);
        if (info->mMediaBuffer != NULL) {
            info->mMediaBuffer->setObserver(NULL);
            info->mMediaBuffer->release();
        }
    }
    mOutBuffers.clear();
    mFilledBuffers.clear();
    mFreeBuffers.clear();

    av_free (pFrame);
    avcodec_close(pCodecCtx);

    mSource->stop();

#ifdef FPS
    float timesec;
    int64_t totalPlayTimeUs;
    totalPlayTimeUs = mSystemTimeSource.getRealTimeUs() - beginPlayTimeUs;
    timesec = 	(float)(totalPlayTimeUs/1000000.0);
    LOGI("<<< Statistics in FFmpegs Solution:");
    LOGI("<<< Decode frame num = %lld, max_interval = %lldus, min_interval = %lldus, avr_interval = %lldus;\n",
            mDecFrameNum, mDecMaxInterval, mDecMinInterval, mDecSumInterval/mDecFrameNum);
    LOGI("<<< Copy frame num = %lld, max_interval = %lldus, min_interval = %lldus, avr_interval = %lldus;\n",
            mCpyFrameNum, mCpyMaxInterval, mCpyMinInterval, mCpySumInterval/mCpyFrameNum);
    LOGI("<<< Elapse time = %2f s, FPS = %.2f fps", timesec, (float)(mDecFrameNum/timesec));	
#endif	

    LOGI("%s ...  OK ", __func__);

    return OK;
}

sp<MetaData> FfmpegVideoDecoder::getFormat() {
    Mutex::Autolock autoLock(mLock);
    LOGI("%s ... ", __func__);

    return mFormat;
}

void *FfmpegVideoDecoder::ThreadWrapper(void * me) {
    FfmpegVideoDecoder *videodecoder = static_cast<FfmpegVideoDecoder *>(me);

    status_t err = videodecoder->decode();
    return (void *) err;
}

status_t FfmpegVideoDecoder::decode () {
    status_t err = OK;

#ifdef FPS
    int64_t prevDecTimeUs;
#endif

    while (mStarted) {
        {
            Mutex::Autolock autoLock(mLock);
            while(mFreeBuffers.empty() && mStarted) {
                mBufferFreed.wait(mLock);
            }
        }

        if (!mStarted) {
            err = OK;
            LOGI ("stop decode thread ...... ");
            goto decode_return;
        }

#ifdef FPS
        prevDecTimeUs = mSystemTimeSource.getRealTimeUs();
#endif

        Mutex::Autolock autoLock(mLock);
        err = OK;
        if (mSeekTimeUs >= 0) {
            LOGI(" %s seek to %lld us ... ", __func__, mSeekTimeUs);
            /* flush and seek */
            if (mInputBuffer) {
                mInputBuffer->release();
                mInputBuffer = NULL;
            }
            avcodec_flush_buffers(pCodecCtx);
            MediaSource::ReadOptions options;
            options.setSeekTo(mSeekTimeUs, mSeekMode);

            mSeekTimeUs = -1;
            mSeekMode = ReadOptions::SEEK_CLOSEST_SYNC;
            mBufferFilled.signal();
            err = mSource->read(&mInputBuffer, &options);

            int64_t targetTimeUs;
            if (mInputBuffer->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs) && targetTimeUs >= 0) {
                mTargetTimeUs = targetTimeUs;
                mLastTimeUs = mTargetTimeUs - 1;
            } else {
                mTargetTimeUs = -1;
            }
        } else {
            if (mInputBuffer == NULL) {
                /* normal read */
                err = mSource->read(&mInputBuffer);
            }
        }

        if (err != OK) {
            /* output err frame */
            mFinalStatus = ERROR_END_OF_STREAM;
            mBufferFilled.signal();

#ifdef FPS
            LOGI(" decode frame num = %lld, max_interval = %lld, min_interval = %lld, avr_interval = %lld\n",
                    mDecFrameNum, mDecMaxInterval, mDecMinInterval, mDecSumInterval/mDecFrameNum);
            LOGI(" copy frame num = %lld, max_interval = %lld, min_interval = %lld, avr_interval = %lld\n",
                    mCpyFrameNum, mCpyMaxInterval, mCpyMinInterval, mCpySumInterval/mCpyFrameNum);
#endif

            LOGI ("maybe end of stream ...... ");
            goto decode_return;
        }

        uint8_t *bitstream = (uint8_t *) mInputBuffer->data() + mInputBuffer->range_offset();
        int32_t bufferSize = mInputBuffer->range_length();
        int32_t frameFinished = 0;
        int64_t timeUs = 0;

        CHECK(mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
        if (timeUs <= mLastTimeUs)
        {
            timeUs = mLastTimeUs;
        }
        //LOGI("pts = %lld ... ", timeUs);
        mLastTimeUs = timeUs;

        AVPacket avpkt;
        av_init_packet (&avpkt);
        avpkt.data = bitstream;
        avpkt.size = bufferSize;

        int32_t decodesize = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &avpkt);
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
            int     numBytes;
            uint8_t *buffer;

#ifdef FPS
            int64_t curIntervalUs;

            curIntervalUs = mSystemTimeSource.getRealTimeUs() - prevDecTimeUs;
            if (curIntervalUs > mDecMaxInterval) {
                mDecMaxInterval = curIntervalUs;
            }
            if (curIntervalUs < mDecMinInterval) {
                mDecMinInterval = curIntervalUs;
            }
            mDecSumInterval += curIntervalUs;
            mDecFrameNum++;
#endif
            if ((pCodecCtx->width != mImgWidth) || (pCodecCtx->height != mImgHeight)) {
                mImgWidth = pCodecCtx->width;
                mImgHeight = pCodecCtx->height;
                mBufWidth = ((mImgWidth+15) & ~15);
                mBufHeight = ((mImgHeight+15) & ~15);
                mFormat->setInt32(kKeyWidth, mBufWidth);
                mFormat->setInt32(kKeyHeight, mBufHeight);
                mFormat->setInt32(kKeyDisplayWidth, mImgWidth);
                mFormat->setInt32(kKeyDisplayHeight, mImgHeight);
                mFormat->setRect(kKeyCropRect, 0, 0, mImgWidth - 1, mImgHeight - 1);

                LOGI("information changed : disp width = %d, disp height = %d ... ", mImgWidth, mImgHeight);

                if ((mBufWidth * mBufHeight) > MAX_SUPPORT_RESOLUTION) {
                    mFinalStatus =  ERROR_UNSUPPORTED;
                    mBufferFilled.signal();
                    goto decode_return;
                }

#ifdef FFMPEG_USER_NATIVEWINDOW_RENDER
                //cancel all buffer first
                if (mNativeWindow != NULL) {
                    while (!mFreeBuffers.empty()) {
                        size_t index = *mFreeBuffers.begin();
                        mFreeBuffers.erase(mFreeBuffers.begin());
                        OutBufferInfo *info = &mOutBuffers.editItemAt(index);
                        status_t tmp = mNativeWindow->cancelBuffer(
                                mNativeWindow.get(), info->mMediaBuffer->graphicBuffer().get());
                        if (tmp != OK) {
                            LOGE("cancelBuffer failed: %s (%d)", strerror(-tmp), -tmp);
                        }
                    }
                }
#endif
                err = initMediaBuffer();
                if (err != OK) {
                    LOGE ("initMediaBuffer error ... ");
                    mFinalStatus =  err;
                    mBufferFilled.signal();
                    goto decode_return;
                }

                mFinalStatus = INFO_FORMAT_CHANGED;
                mBufferFilled.signal();
                continue;
            }

            bool skipFrame = false;
            if (mTargetTimeUs >= 0) {
                if (timeUs < mTargetTimeUs) {
                    skipFrame = true;
                    LOGI("skipping frame at %lld us", timeUs);
                } else {
                    LOGI("found target frame at %lld us", timeUs);
                    mTargetTimeUs = -1;
                }
            }

            if (!skipFrame) {
                if (pCodecCtx->pix_fmt == PIX_FMT_YUV420P) {
                    AVFrame *pFrameOut = avcodec_alloc_frame();
                    CHECK (pFrameOut != NULL);

                    size_t index = *mFreeBuffers.begin();
                    mFreeBuffers.erase(mFreeBuffers.begin());
                    OutBufferInfo *info = &mOutBuffers.editItemAt(index);

                    MediaBuffer *pOutputMediaBuffer = info->mMediaBuffer;
                    CHECK (pOutputMediaBuffer != NULL);
                    pOutputMediaBuffer->set_range(0,  info->mSize);
                    pOutputMediaBuffer->meta_data()->clear();
                    pOutputMediaBuffer->meta_data()->setInt64(kKeyTime, mLastTimeUs);

#ifdef FPS
                    int64_t prevCpyTimeUs = mSystemTimeSource.getRealTimeUs();
#endif

#ifdef FFMPEG_USER_NATIVEWINDOW_RENDER
                    //LOGI("decode index %d ... ", index);
                    if (mNativeWindow != NULL) {
                        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
                        buffer_handle_t bufferHandle = (buffer_handle_t) (pOutputMediaBuffer->graphicBuffer()->handle);
                        Rect bounds(mBufWidth, mBufHeight);
                        void *vaddr[3];

                        int usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_YUV_ADDR;
                        if (mapper.lock(bufferHandle, usage, bounds, vaddr) != 0) {
                            LOGE("graphicbuffermapper.lock error");
                            mFinalStatus = ERROR_IO;
                            mBufferFilled.signal();
                            break;
                        }

                        pFrameOut->linesize[0] = mBufWidth;
                        pFrameOut->linesize[1] = mBufWidth >> 1;
                        pFrameOut->linesize[2] = mBufWidth >> 1;
                        pFrameOut->data[0] = (uint8_t *)(vaddr[0]);
                        pFrameOut->data[1] = (uint8_t *)(vaddr[1]);
                        pFrameOut->data[2] = (uint8_t *)(vaddr[2]);
                        av_picture_copy ((AVPicture *)pFrameOut, (AVPicture*)pFrame, PIX_FMT_YUV420P, mImgWidth, mImgHeight);

                        if (mapper.unlock(bufferHandle) != 0) {
                            LOGE("graphicbuffermapper.unlock error");
                            mFinalStatus = ERROR_IO;
                            mBufferFilled.signal();
                            break;
                        }
                    }
                    else 
#endif
                    {
                        avpicture_fill((AVPicture *)pFrameOut, (uint8_t *)(pOutputMediaBuffer->data()), PIX_FMT_YUV420P, mBufWidth, mBufHeight);
                        av_picture_copy ((AVPicture *)pFrameOut, (AVPicture*)pFrame, PIX_FMT_YUV420P, mImgWidth, mImgHeight);
                    }
                    av_free (pFrameOut);

#ifdef FPS
                    int64_t curIntervalUs;

                    curIntervalUs = mSystemTimeSource.getRealTimeUs() - prevCpyTimeUs;
                    if (curIntervalUs > mCpyMaxInterval) {
                        mCpyMaxInterval = curIntervalUs;
                    }
                    if (curIntervalUs < mCpyMinInterval) {
                        mCpyMinInterval = curIntervalUs;
                    }
                    mCpySumInterval += curIntervalUs;
                    mCpyFrameNum++;
#endif

                    mFilledBuffers.push_back(index);
                    mBufferFilled.signal();
                }
            }
        }
    }

decode_return:

    return err;
}

status_t FfmpegVideoDecoder::read (MediaBuffer **out, const ReadOptions *options) {
    Mutex::Autolock autoLock(mLock);

    status_t err;
    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        mSeekTimeUs = seekTimeUs;
        mSeekMode = mode;
        while (!mFilledBuffers.empty()) {
           size_t index = *mFilledBuffers.begin();
           mFilledBuffers.erase(mFilledBuffers.begin());
           OutBufferInfo *info = &mOutBuffers.editItemAt(index);
           mFreeBuffers.push_back(index);
           mBufferFreed.signal();
        }

        LOGI("FfmpegVideoDecoder::read : seek to %lldus ... ", mSeekTimeUs);

        //wait for response to the seek operation
        while (mSeekTimeUs >= 0) {
            if ((err = mBufferFilled.waitRelative(mLock, MAX_DECODE_WAIT_TIME)) != OK) {
                LOGE("FfmpegVideoDecoder::read : waiting for seek operation time out ... ");
                return err;
            }
        }
    }

    while (mFilledBuffers.empty() && mStarted && (mFinalStatus == OK)) {
        if ((err = mBufferFilled.waitRelative(mLock, MAX_DECODE_WAIT_TIME)) != OK) {
            LOGE("FfmpegVideoDecoder::read : waiting for decode time out ... ");
            return err;
        }
    }

    if (mFinalStatus != OK) {
        LOGI("FfmpegVideoDecoder::read : mFinalStatus = %d ... ", mFinalStatus);
        err = mFinalStatus;
        mFinalStatus = OK;
        return err;
    }

    if (!mStarted) {
        LOGI("FfmpegVideoDecoder::read : stop ... ");
        return ERROR_END_OF_STREAM;
    }

    size_t index = *mFilledBuffers.begin();
    mFilledBuffers.erase(mFilledBuffers.begin());

    OutBufferInfo *info = &mOutBuffers.editItemAt(index);
    info->mMediaBuffer->add_ref();
    *out = info->mMediaBuffer;

    return OK;
}

void FfmpegVideoDecoder::signalBufferReturned(MediaBuffer *buffer) {
    Mutex::Autolock autoLock(mLock);
    size_t index;
    OutBufferInfo *info;
    for (index = 0; index < mOutBuffers.size(); index++) {
        info = &mOutBuffers.editItemAt(index);
        if (info->mMediaBuffer == buffer) {
            break;
        }
    }
    CHECK (index < mOutBuffers.size());

    if (buffer->graphicBuffer() == 0) {
        mFreeBuffers.push_back(index);
        mBufferFreed.signal();
    } else {

        //LOGI("graphic buffer release, index = %d", index);

        sp<MetaData> metaData = info->mMediaBuffer->meta_data();
        int32_t rendered = 0;
        if (!metaData->findInt32(kKeyRendered, &rendered)) {
            rendered = 0;
        }

        if (!rendered) {
            LOGI("graphic buffer not queued, index = %d", index);
            mFreeBuffers.push_back(index);
            mBufferFreed.signal();
            return;
        }

        ANativeWindowBuffer* buf;
        int err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf);
        if (err != 0) {
            LOGE("FfmpegVideoDecoder::signalBufferReturned: dequeueBuffer failed w/ error 0x%08x", err);
            return;
        }

        // Determine which buffer we just dequeued.
        size_t i;
        for (i = 0; i < mOutBuffers.size(); i++) {
            sp<GraphicBuffer> graphicBuffer = (&mOutBuffers.editItemAt(i))->mMediaBuffer->graphicBuffer();
            if (graphicBuffer->handle == buf->handle) {
                break;
            }
        }

        if (i >= mOutBuffers.size()) {
            LOGE("FfmpegVideoDecoder::signalBufferReturned: dequeued unrecognized buffer ... ");
            return;
        }
        //LOGI("graphic buffer dequeue index = %d", i);
        mFreeBuffers.push_back(i);
        mBufferFreed.signal();
    }
}

}  // namespace android
#endif
