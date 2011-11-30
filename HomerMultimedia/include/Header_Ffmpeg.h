/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: header includes for ffmpeg library
 * Author:  Thomas Volkert
 * Since:   2009-02-16
 */

#ifndef _MULTIMEDIA_HEADER_FFMPEG_
#define _MULTIMEDIA_HEADER_FFMPEG_

#if __GNUC__ >=3
#pragma GCC system_header //suppress warnings from ffmpeg
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <libavutil/fifo.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

#if LIBAVFORMAT_VERSION_MAJOR < 50
    #define AV_NEW_FORMAT_CONTEXT av_alloc_format_context
#else
    #define AV_NEW_FORMAT_CONTEXT avformat_alloc_context
#endif

#if LIBAVFORMAT_VERSION_INT <= AV_VERSION_INT(52, 64, 2)
    #define AV_GUESS_FORMAT guess_format
#else
    #define AV_GUESS_FORMAT av_guess_format
#endif

#ifndef CodecType
#define CodecType CodecID
#endif

#if LIBAVUTIL_VERSION_MAJOR < 51
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#define AVMEDIA_TYPE_UNKNOWN CODEC_TYPE_UNKNOWN
#endif

inline int HM_av_metadata_set(AVDictionary **pm, const char *key, const char *value)
{
    #if LIBAVFORMAT_VERSION_INT <= AV_VERSION_INT(52, 64, 2)
        return av_metadata_set(pm, key, value);
    #else
        return av_metadata_set2(pm, key, value, AV_METADATA_MATCH_CASE);
    #endif
}

inline int HM_avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, AVPacket *avpkt)
{
    #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
        return avcodec_decode_video(avctx, picture, got_picture_ptr, avpkt->data, avpkt->size);
    #else
        return avcodec_decode_video2(avctx, picture, got_picture_ptr, avpkt);
    #endif
}

inline int HM_avcodec_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt)
{
    #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
        return avcodec_decode_audio2(avctx, samples, frame_size_ptr, avpkt->data, avpkt->size);
    #else
        return avcodec_decode_audio3(avctx, samples, frame_size_ptr, avpkt);
    #endif
}

inline AVFifoBuffer *HM_av_fifo_alloc(unsigned int size)
{
    #if LIBAVUTIL_VERSION_MAJOR < 50
        AVFifoBuffer *tFifo = malloc(sizeof(AVFifoBuffer));
        av_fifo_init(tFifo, size);
        return tFifo;
    #else
        return av_fifo_alloc(size);
    #endif
}

inline int HM_av_fifo_generic_read(AVFifoBuffer *f, void *dest, int buf_size)
{
    #if LIBAVUTIL_VERSION_MAJOR < 50
        return av_fifo_generic_read(f, buf_size, NULL, dest);
    #else
        return av_fifo_generic_read(f, dest, buf_size, NULL);
    #endif
}

inline int HM_sws_scale(struct SwsContext *context, const uint8_t* const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[], const int dstStride[])
{
    #if LIBSWSCALE_VERSION_MAJOR < 1
        return sws_scale(context, (const uint8_t**)srcSlice, (int*)&srcStride[0], srcSliceY, srcSliceH, (uint8_t**)&dst[0], (int*)dstStride[0]);
    #else
        return sws_scale(context, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
    #endif
}

#endif
