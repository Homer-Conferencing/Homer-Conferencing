/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audioconvert.h>
#include <libavutil/avstring.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/random_seed.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#if (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 61, 100))
#include <libswresample/swresample.h>
#endif
}

#ifndef AV_NUM_DATA_POINTERS
#define AV_NUM_DATA_POINTERS 			4 // old value was 4, later "AV_NUM_DATA_POINTERS" was introduced
#endif

#ifndef CODEC_FLAG2_SHOW_ALL
#define CODEC_FLAG2_SHOW_ALL      0x00400000 ///< Show all frames before the first keyframe
#endif

#if (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 61, 100))

#define HM_SwrContext                       SwrContext

inline struct SwrContext *HM_swr_alloc_set_opts(struct SwrContext *s,
                                      int64_t out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
                                      int64_t  in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate,
                                      int log_offset, void *log_ctx)
{
    return swr_alloc_set_opts(NULL,
                              out_ch_layout, out_sample_fmt, out_sample_rate,
                              in_ch_layout, in_sample_fmt, in_sample_rate,
                              log_offset, log_ctx);
}

inline int HM_swr_init(struct SwrContext *s)
{
	return swr_init(s);
}

inline int HM_swr_convert(struct SwrContext *s, uint8_t **out, int out_count,
        const uint8_t **in , int in_count)
{
    return swr_convert(s, out, out_count, in, in_count);
}

inline void HM_swr_free(struct SwrContext **s)
{
	swr_free(s);
}

#else

#define HM_SwrContext                       ReSampleContext

inline struct ReSampleContext *HM_swr_alloc_set_opts(struct ReSampleContext *s,
                                      int64_t out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
                                      int64_t  in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate,
                                      int log_offset, void *log_ctx)
{
    return av_audio_resample_init(av_get_channel_layout_nb_channels(out_ch_layout), av_get_channel_layout_nb_channels(in_ch_layout),
            out_sample_rate, in_sample_rate,
            out_sample_fmt,
            in_sample_fmt,
            16, 10,
            0, 0.8);
}

inline int HM_swr_init(struct ReSampleContext *s)
{
	return 0;
}

inline int HM_swr_convert(struct ReSampleContext *s, uint8_t **out, int out_count,
        const uint8_t **in , int in_count)
{
    return audio_resample(s, (short*)*out, (short*)*in, in_count);
}

inline void HM_swr_free(struct ReSampleContext **s)
{
	audio_resample_close(*s);
}

#endif

#if (LIBAVFORMAT_VERSION_MAJOR < 50)
    #define AV_NEW_FORMAT_CONTEXT av_alloc_format_context
#else
    #define AV_NEW_FORMAT_CONTEXT avformat_alloc_context
#endif

#if (LIBAVFORMAT_VERSION_INT <= AV_VERSION_INT(52, 64, 2))
    #define AV_GUESS_FORMAT guess_format
#else
    #define AV_GUESS_FORMAT av_guess_format
#endif

#ifndef APPLE // on OSX we would have redefinitions and conflicts with QuickTime
	#ifndef CodecType
		#define CodecType CodecID
	#endif
#endif

#if (LIBAVUTIL_VERSION_MAJOR < 51)
    #define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
    #define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
    #define AVMEDIA_TYPE_UNKNOWN CODEC_TYPE_UNKNOWN
#endif

inline int HM_avformat_write_header(AVFormatContext *s)
{
    #if LIBAVFORMAT_VERSION_MAJOR < 54
        return av_write_header(s);
    #else
        return avformat_write_header(s, NULL);
    #endif
}

inline int HM_av_dict_set(AVDictionary **pm, const char *key, const char *value)
{
    #if (LIBAVFORMAT_VERSION_MAJOR < 54)
        #if LIBAVFORMAT_VERSION_INT <= AV_VERSION_INT(52, 64, 2)
            return av_metadata_set(pm, key, value);
        #else
            return av_metadata_set2(pm, key, value, AV_METADATA_MATCH_CASE);
        #endif
    #else
        return av_dict_set(pm, key, value, AV_DICT_MATCH_CASE);
    #endif
}

inline AVDictionaryEntry* HM_av_dict_get(AVDictionary *pm, const char *key, const AVDictionaryEntry *prev)
{
    #if (LIBAVFORMAT_VERSION_MAJOR < 54)
        return av_metadata_get(pm, key, prev, AV_DICT_IGNORE_SUFFIX);
    #else
        return av_dict_get(pm, key, prev, AV_DICT_IGNORE_SUFFIX);
    #endif
}

inline int HM_avcodec_open(AVCodecContext *avctx, AVCodec *codec, AVDictionary **options)
{
    #if (LIBAVCODEC_VERSION_MAJOR < 54)
        return avcodec_open(avctx, codec);
    #else
        return avcodec_open2(avctx, codec, options);
    #endif
}

inline int HM_avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, AVPacket *avpkt)
{
    #if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0))
        return avcodec_decode_video(avctx, picture, got_picture_ptr, avpkt->data, avpkt->size);
    #else
        return avcodec_decode_video2(avctx, picture, got_picture_ptr, avpkt);
    #endif
}

inline int HM_avcodec_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt)
{
    #if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0))
        return avcodec_decode_audio2(avctx, samples, frame_size_ptr, avpkt->data, avpkt->size);
    #else
        return avcodec_decode_audio3(avctx, samples, frame_size_ptr, avpkt);
    #endif
}

inline AVFifoBuffer *HM_av_fifo_alloc(unsigned int size)
{
    #if (LIBAVUTIL_VERSION_MAJOR < 50)
        AVFifoBuffer *tFifo = malloc(sizeof(AVFifoBuffer));
        av_fifo_init(tFifo, size);
        return tFifo;
    #else
        return av_fifo_alloc(size);
    #endif
}

inline int HM_av_fifo_generic_read(AVFifoBuffer *f, void *dest, int buf_size)
{
    #if (LIBAVUTIL_VERSION_MAJOR < 50)
        return av_fifo_generic_read(f, buf_size, NULL, dest);
    #else
        return av_fifo_generic_read(f, dest, buf_size, NULL);
    #endif
}

inline int HM_sws_scale(struct SwsContext *context, const uint8_t* const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[], const int dstStride[])
{
    #if (LIBSWSCALE_VERSION_MAJOR < 1)
        return sws_scale(context, (const uint8_t**)srcSlice, (int*)&srcStride[0], srcSliceY, srcSliceH, (uint8_t**)&dst[0], (int*)dstStride[0]);
    #else
        return sws_scale(context, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
    #endif
}

inline void HM_avformat_close_input(AVFormatContext *s)
{
	#if (LIBAVFORMAT_VERSION_MAJOR < 54)
		av_close_input_file(s);
	#else
		avformat_close_input(&s);
	#endif
}

inline AVStream *HM_avformat_new_stream(AVFormatContext *s, int id)
{
	#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 10, 0))
		return av_new_stream(s, c);
	#else
		AVStream *st = avformat_new_stream(s, NULL);
		if (st)
			st->id = id;
		return st;
	#endif
}

#if (LIBAVUTIL_VERSION_INT < AV_VERSION_INT(51, 35, 0))
// ffmpeg would say: "better than nothing"
inline int av_opt_set(void *obj, const char *name, const char *val, int search_flags)
{
    return 0;
}
#endif
#endif
