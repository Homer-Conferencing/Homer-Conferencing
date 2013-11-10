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

/*
 * This file is needed to maintain compatibility to the different APIs of known ffmpeg releases.
 * This includes release 0.10.*, 0.11.*, 1.0.1, 1.1. This excludes release 0.8.*!
 * Starting from January 2013, we won't continue this work and use error messages instead in order
 * to give hints which version of ffmpeg would work.
 */

#ifndef _MULTIMEDIA_HEADER_FFMPEG_
#define _MULTIMEDIA_HEADER_FFMPEG_

#if __GNUC__ >=3
#pragma GCC system_header //suppress warnings from ffmpeg
#endif

#include <Logger.h>
using namespace Homer::Base;

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
#if (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(54, 5, 2))
#include <libavutil/time.h>
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

#if (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 25, 0))
    #define AVCODEC_MAX_AUDIO_FRAME_SIZE   192000
#endif
 
#ifndef AV_PICTURE_TYPE_NONE
#define AV_PICTURE_TYPE_NONE ((enum AVPictureType)0)
#endif

inline int64_t HM_av_get_default_channel_layout(int nb_channels)
{
    #if (LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 13, 100))
        switch(nb_channels)
        {
            case 1:
                return AV_CH_LAYOUT_MONO;
            case 2:
                return AV_CH_LAYOUT_STEREO;
            case 6:
                return AV_CH_LAYOUT_5POINT1;
            case 8:
                return AV_CH_LAYOUT_7POINT1;
            default:
                return AV_CH_LAYOUT_STEREO;
        }
    #else
        return av_get_default_channel_layout(nb_channels);
    #endif
}

#if (LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(54, 51, 100))
    #define AVCodecID CodecID
    #define AV_CODEC_ID_AAC CODEC_ID_AAC
    #define AV_CODEC_ID_AC3 CODEC_ID_AC3
    #define AV_CODEC_ID_ADPCM CODEC_ID_ADPCM
    #define AV_CODEC_ID_ADPCM_G722 CODEC_ID_ADPCM_G722
    #define AV_CODEC_ID_AMR CODEC_ID_AMR
    #define AV_CODEC_ID_AMR_NB CODEC_ID_AMR_NB
    #define AV_CODEC_ID_GSM CODEC_ID_GSM
    #define AV_CODEC_ID_H261 CODEC_ID_H261
    #define AV_CODEC_ID_H263 CODEC_ID_H263
    #define AV_CODEC_ID_H263P CODEC_ID_H263P
    #define AV_CODEC_ID_H264 CODEC_ID_H264
    #define AV_CODEC_ID_MJPEG CODEC_ID_MJPEG
    #define AV_CODEC_ID_MP3 CODEC_ID_MP3
    #define AV_CODEC_ID_MPEG1VIDEO CODEC_ID_MPEG1VIDEO
    #define AV_CODEC_ID_MPEG2TS CODEC_ID_MPEG2TS
    #define AV_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
    #define AV_CODEC_ID_MPEG4 CODEC_ID_MPEG4
    #define AV_CODEC_ID_NONE CODEC_ID_NONE
    #define AV_CODEC_ID_PCM CODEC_ID_PCM
    #define AV_CODEC_ID_PCM_ALAW CODEC_ID_PCM_ALAW
    #define AV_CODEC_ID_PCM_MULAW CODEC_ID_PCM_MULAW
    #define AV_CODEC_ID_PCM_S16BE CODEC_ID_PCM_S16BE
    #define AV_CODEC_ID_PCM_S16LE CODEC_ID_PCM_S16LE
    #define AV_CODEC_ID_THEORA CODEC_ID_THEORA
    #define AV_CODEC_ID_VORBIS CODEC_ID_VORBIS
    #define AV_CODEC_ID_VP8 CODEC_ID_VP8
    #define AV_CODEC_ID_WMAV2 CODEC_ID_WMAV2
    #define AV_CODEC_ID_WMV3 CODEC_ID_WMV3
#endif

inline const char *HM_avcodec_get_name(enum AVCodecID id)
{
    #if (LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(53, 61, 100))
        return "N/A"; //TODO: add some implementation here?
    #else
        return avcodec_get_name(id);
    #endif
}

inline int HM_av_samples_fill_arrays(uint8_t **audio_data, int *linesize, uint8_t *buf, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align)
{
    #if (LIBAVUTIL_VERSION_INT < AV_VERSION_INT(51, 54, 100))
        return av_samples_fill_arrays(audio_data, linesize, buf, nb_channels, nb_samples, sample_fmt, align);
    #else
        return av_samples_fill_arrays(audio_data, linesize, (const uint8_t*)buf, nb_channels, nb_samples, sample_fmt, align);
    #endif
}

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
        int tResult = avcodec_decode_video(avctx, picture, got_picture_ptr, avpkt->data, avpkt->size);
        if ((got_picture_ptr != NULL) && (tResult >= 0))
            *got_picture_ptr = 1;
        return tResult;
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

inline int HM_avcodec_encode_video2(AVCodecContext *avctx, AVPacket *avpkt, const AVFrame *frame, int *got_packet_ptr)
{
    int tResult;

    #if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 23, 100))
		if (avpkt == NULL)
			return -1;

		if (avpkt->data == NULL)
		{
			avpkt->size = avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height) + FF_INPUT_BUFFER_PADDING_SIZE;
			avpkt->data = (uint8_t*)malloc(avpkt->size);
		}

		tResult = avcodec_encode_video(avctx, avpkt->data, avpkt->size, (const AVFrame *)frame);

        if (frame->key_frame)
            avpkt->flags |= AV_PKT_FLAG_KEY;

        avpkt->pts = frame->pts;
        avpkt->dts = frame->pts;
        avpkt->size = tResult;

		if (got_packet_ptr != NULL)
			*got_packet_ptr = (tResult > 0);
    #else
        tResult = avcodec_encode_video2(avctx, avpkt, frame, got_packet_ptr);
    #endif

	if ((!avctx) || (!(avctx->codec->capabilities & CODEC_CAP_DELAY)))
	{// no delay by encoder
		avpkt->pts = frame->pts;
		avpkt->dts = frame->pts;
	}else
	{// possible delay by encoder, e.g., H.264 encoder
		// encoder has set the pts/dts value during "avcodec_encode_video(2)"
	}

    return tResult;
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
		return av_new_stream(s, id);
	#else
		AVStream *st = avformat_new_stream(s, NULL);
		if (st)
			st->id = id;
		return st;
	#endif
}

#if (LIBAVUTIL_VERSION_INT < AV_VERSION_INT(51, 35, 0))
inline int av_opt_set(void *obj, const char *name, const char *val, int search_flags)
{
    return av_set_string3(obj, name, val, 0, NULL);
}
#endif

inline int64_t HM_av_frame_get_best_effort_timestamp(const AVFrame *frame)
{
    #if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 23, 100))
        int64_t tResult = 0;
        if (frame->pkt_dts != (int64_t)AV_NOPTS_VALUE)
        {// use DTS value from decoder
            tResult = frame->pkt_dts;
        }else if (frame->pkt_pts != (int64_t)AV_NOPTS_VALUE)
        {// fall back to reordered PTS value
            tResult = frame->pkt_pts;
        }
        return tResult;
    #else
        return av_frame_get_best_effort_timestamp(frame);
    #endif
}

#endif
