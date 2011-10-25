/*
 * Name:    Header_Ffmpeg.h
 * Purpose: header includes for ffmpeg library
 * Author:  Thomas Volkert
 * Since:   2009-02-16
 * Version: $Id: Header_Ffmpeg.h,v 1.6 2011/10/20 15:31:40 chaos Exp $
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
#endif
