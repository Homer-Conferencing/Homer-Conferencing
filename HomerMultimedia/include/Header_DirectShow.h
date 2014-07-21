/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: header includes and definitions for DirectShow
 * Since:   2012-11-09
 */

#ifndef _MULTIMEDIA_HEADER_DIRECTSHOW_
#define _MULTIMEDIA_HEADER_DIRECTSHOW_

#if defined(WINDOWS)
//#pragma GCC system_header

///////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <objidl.h>
#include <strmif.h> // remove this in case of definition conflicts

#define COM_NO_WINDOWS_H
#include <oaidl.h>
#include <ocidl.h>
#include <winnls.h>

///////////////////////////////////////////////////////////////////////////////

// Bdamedia.h filter categories
DEFINE_GUID( KSCATEGORY_BDA_RECEIVER_COMPONENT, 0xFD0A5AF4, 0xB41D, 0x11d2, 0x9c, 0x95, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSCATEGORY_BDA_NETWORK_TUNER, 0x71985f48, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSCATEGORY_BDA_NETWORK_EPG, 0x71985f49, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSCATEGORY_IP_SINK, 0x71985f4aL, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x00, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSCATEGORY_BDA_NETWORK_PROVIDER, 0x71985f4b, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSCATEGORY_BDA_TRANSPORT_INFORMATION, 0xa2e3074f, 0x6c3d, 0x11d3, 0xb6, 0x53, 0x0, 0xc0, 0x4f, 0x79, 0x49, 0x8e);

// Bdamedia.h stream formats
DEFINE_GUID( KSDATAFORMAT_TYPE_BDA_ANTENNA, 0x71985f41, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, 0xf4aeb342, 0x0329, 0x4fdd, 0xa8, 0xfd, 0x4a, 0xff, 0x49, 0x26, 0xc9, 0x78);
DEFINE_GUID( KSDATAFORMAT_SPECIFIER_BDA_TRANSPORT, 0x8deda6fd, 0xac5f, 0x4334, 0x8e, 0xcf, 0xa4, 0xba, 0x8f, 0xa7, 0xd0, 0xf0);
DEFINE_GUID( KSDATAFORMAT_TYPE_BDA_IF_SIGNAL, 0x61be0b47, 0xa5eb, 0x499b, 0x9a, 0x85, 0x5b, 0x16, 0xc0, 0x7f, 0x12, 0x58);
DEFINE_GUID( KSDATAFORMAT_TYPE_MPEG2_SECTIONS, 0x455f176c, 0x4b06, 0x47ce, 0x9a, 0xef, 0x8c, 0xae, 0xf7, 0x3d, 0xf7, 0xb5);
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_ATSC_SI, 0xb3c7397c, 0xd303, 0x414d, 0xb3, 0x3c, 0x4e, 0xd2, 0xc9, 0xd2, 0x97, 0x33);
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_DVB_SI, 0xe9dd31a3, 0x221d, 0x4adb, 0x85, 0x32, 0x9a, 0xf3, 0x9, 0xc1, 0xa4, 0x8);
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_BDA_OPENCABLE_PSIP, 0x762e3f66, 0x336f, 0x48d1, 0xbf, 0x83, 0x2b, 0x0, 0x35, 0x2c, 0x11, 0xf0);
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_BDA_OPENCABLE_OOB_PSIP, 0x951727db, 0xd2ce, 0x4528, 0x96, 0xf6, 0x33, 0x1, 0xfa, 0xbb, 0x2d, 0xe0);

//  Uuids.h BDA network provider
DEFINE_GUID( CLSID_ATSCNetworkProvider, 0x0dad2fdd, 0x5fd7, 0x11d3, 0x8f, 0x50, 0x00, 0xc0, 0x4f, 0x79, 0x71, 0xe2);
DEFINE_GUID( CLSID_ATSCNetworkPropertyPage, 0xe3444d16, 0x5ac4, 0x4386, 0x88, 0xdf, 0x13, 0xfd, 0x23, 0x0e, 0x1d, 0xda);
DEFINE_GUID( CLSID_DVBSNetworkProvider, 0xfa4b375a, 0x45b4, 0x4d45, 0x84, 0x40, 0x26, 0x39, 0x57, 0xb1, 0x16, 0x23);
DEFINE_GUID( CLSID_DVBTNetworkProvider, 0x216c62df, 0x6d7f, 0x4e9a, 0x85, 0x71, 0x5, 0xf1, 0x4e, 0xdb, 0x76, 0x6a);
DEFINE_GUID( CLSID_DVBCNetworkProvider, 0xdc0c0fe7, 0x485, 0x4266, 0xb9, 0x3f, 0x68, 0xfb, 0xf8, 0xe, 0xd8, 0x34);

// interface identifiers for BDA interface
DEFINE_GUID( IID_IBDA_Topology, 0x79B56888, 0x7FEA, 0x4690, 0xB4, 0x5D, 0x38, 0xFD, 0x3C, 0x78, 0x49, 0xBE);
DEFINE_GUID( IID_IBDA_NetworkProvider, 0xfd501041, 0x8ebe, 0x11ce, 0x81, 0x83, 0x00, 0xaa, 0x00, 0x57, 0x7d, 0xa2);
DEFINE_GUID( IID_IBDA_DeviceControl, 0xFD0A5AF3, 0xB41D, 0x11d2, 0x9C, 0x95, 0x00, 0xC0, 0x4F, 0x79, 0x71, 0xE0);
DEFINE_GUID( IID_IBDA_DigitalDemodulator, 0xef30f379, 0x985b, 0x4d10, 0xb6, 0x40, 0xa7, 0x9d, 0x5e, 0x04, 0xe1, 0xe0);
DEFINE_GUID( IID_IBDA_DigitalDemodulator2, 0x525ed3ee, 0x5cf3, 0x4e1e, 0x9a, 0x06, 0x53, 0x68, 0xa8, 0x4f, 0x9a, 0x6e);

// BDA node categories
DEFINE_GUID( KSNODE_BDA_RF_TUNER, 0x71985f4c, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSNODE_BDA_QAM_DEMODULATOR, 0x71985f4d, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSNODE_BDA_QPSK_DEMODULATOR, 0x6390c905, 0x27c1, 0x4d67, 0xbd, 0xb7, 0x77, 0xc5, 0xd, 0x7, 0x93, 0x0);
DEFINE_GUID( KSNODE_BDA_8VSB_DEMODULATOR, 0x71985f4f, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSNODE_BDA_COFDM_DEMODULATOR, 0x2dac6e05, 0xedbe, 0x4b9c, 0xb3, 0x87, 0x1b, 0x6f, 0xad, 0x7d, 0x64, 0x95);
DEFINE_GUID( KSNODE_BDA_OPENCABLE_POD, 0xd83ef8fc, 0xf3b8, 0x45ab, 0x8b, 0x71, 0xec, 0xf7, 0xc3, 0x39, 0xde, 0xb4);
DEFINE_GUID( KSNODE_BDA_COMMON_CA_POD, 0xd83ef8fc, 0xf3b8, 0x45ab, 0x8b, 0x71, 0xec, 0xf7, 0xc3, 0x39, 0xde, 0xb4);
DEFINE_GUID( KSNODE_BDA_PID_FILTER, 0xf5412789, 0xb0a0, 0x44e1, 0xae, 0x4f, 0xee, 0x99, 0x9b, 0x1b, 0x7f, 0xbe);
DEFINE_GUID( KSNODE_IP_SINK, 0x71985f4e, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);

// BDA property sets
DEFINE_GUID( KSPROPSETID_BdaFrequencyFilter, 0x71985f47, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0);
DEFINE_GUID( KSPROPSETID_BdaDigitalDemodulator, 0xef30f379, 0x985b, 0x4d10, 0xb6, 0x40, 0xa7, 0x9d, 0x5e, 0x4, 0xe1, 0xe0);

//  Uuids.h filter categories
DEFINE_GUID( CLSID_VideoInputDeviceCategory, 0x860BB310, 0x5D01, 0x11d0, 0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86);
DEFINE_GUID( CLSID_SystemDeviceEnum, 0x62BE5D10, 0x60EB, 0x11d0, 0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86);
DEFINE_GUID( CLSID_FilterGraph, 0xe436ebb3, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID( CLSID_NullRenderer,0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID( CLSID_VfwCapture, 0x1b544c22, 0xfd0b, 0x11ce, 0x8c, 0x63, 0x0, 0xaa, 0x00, 0x44, 0xb5, 0x1e);

// interface identifiers, old code for compatibility - could be removed in the future
#ifndef __strmif_h__ // ################################## strmif.h ##############################
// property interface
DEFINE_GUID( IID_IKsPropertySet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa, 0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);

DEFINE_GUID( IID_IGraphBuilder, 0x56a868a9, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IEnumPins, 0x56a86892, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IBaseFilter, 0x56a86895, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID( IID_IEnumFilters, 0x56a86893, 0xad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IMediaSample, 0x56a8689a, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);
DEFINE_GUID( IID_IMediaEvent, 0x56a868b6, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IMediaControl, 0x56a868b1, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IAMStreamConfig, 0xc6e13340, 0x30ac, 0x11d0, 0xa1, 0x8c, 0x00, 0xa0, 0xc9, 0x11, 0x89, 0x56);
DEFINE_GUID( IID_IVideoProcAmp, 0x4050560e, 0x42a7, 0x413a, 0x85, 0xc2, 0x09, 0x26, 0x9a, 0x2d, 0x0f, 0x44);
DEFINE_GUID( IID_IFilterGraph, 0x56a8689f, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IFilterGraph2, 0x36b73882, 0xc2c8, 0x11cf, 0x8b, 0x46, 0x00, 0x80, 0x5f, 0x6c, 0xef, 0x60);
#endif // ################################## strmif.h ##############################

// media types
DEFINE_GUID( MEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIATYPE_Interleaved, 0x73766169, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
DEFINE_GUID( MEDIATYPE_VBI, 0xf72a76e1, 0xeb0a, 0x11d0, 0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID( MEDIATYPE_Stream, 0xe436eb83, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);

// media sub types
DEFINE_GUID( MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YV12, 0x32315659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_NV12, 0x3231564E, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IF09, 0x39304649, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IYUV, 0x56555949, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YUYV, 0x56595559, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_AYUV, 0x56555941, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YUY2, 0x32595559, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_UYVY, 0x59565955, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_RGB24, 0xe436eb7d, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( MEDIASUBTYPE_Y211, 0x31313259, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y411, 0x31313459, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y41P, 0x50313459, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YVU9, 0x39555659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YVYU, 0x55595659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_GREY, 0x59455247, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y8, 0x20203859, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y800, 0x30303859, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IMC1, 0x31434d49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IMC2, 0x32434d49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IMC3, 0x33434d49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IMC4, 0x34434d49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID( MEDIASUBTYPE_MPEG2_TRANSPORT, 0xe06d8023, 0xdb46, 0x11cf, 0xb4, 0xd1, 0x00, 0x80, 0x05f, 0x6c, 0xbb, 0xea);

// format types
DEFINE_GUID( FORMAT_VideoInfo, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID( FORMAT_VideoInfo2, 0xf72a76a0, 0xeb0a, 0x11d0, 0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);

///////////////////////////////////////////////////////////////////////////////

typedef LONGLONG REFERENCE_TIME;

typedef struct tagVIDEOINFOHEADER {
        RECT rcSource;
        RECT rcTarget;
        DWORD dwBitRate;
        DWORD dwBitErrorRate;
        REFERENCE_TIME AvgTimePerFrame;
        BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER;

typedef struct tagVIDEOINFOHEADER2 {
        RECT rcSource;
        RECT rcTarget;
        DWORD dwBitRate;
        DWORD dwBitErrorRate;
        REFERENCE_TIME AvgTimePerFrame;
        DWORD dwInterlaceFlags;
        DWORD dwCopyProtectFlags;
        DWORD dwPictAspectRatioX;
        DWORD dwPictAspectRatioY;
        DWORD dwReserved1;
        DWORD dwReserved2;
        BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER2;

#ifndef __strmif_h__ // ################################## strmif.h ##############################

typedef struct _AMMediaType {
        GUID majortype;
        GUID subtype;
        BOOL bFixedSizeSamples;
        BOOL bTemporalCompression;
        ULONG lSampleSize;
        GUID formattype;
        IUnknown *pUnk;
        ULONG cbFormat;
        BYTE *pbFormat;
} AM_MEDIA_TYPE;

typedef struct _VIDEO_STREAM_CONFIG_CAPS
{
    GUID guid;
    ULONG VideoStandard;
    SIZE InputSize;
    SIZE MinCroppingSize;
    SIZE MaxCroppingSize;
    int CropGranularityX;
    int CropGranularityY;
    int CropAlignX;
    int CropAlignY;
    SIZE MinOutputSize;
    SIZE MaxOutputSize;
    int OutputGranularityX;
    int OutputGranularityY;
    int StretchTapsX;
    int StretchTapsY;
    int ShrinkTapsX;
    int ShrinkTapsY;
    LONGLONG MinFrameInterval;
    LONGLONG MaxFrameInterval;
    LONG MinBitsPerSecond;
    LONG MaxBitsPerSecond;
} VIDEO_STREAM_CONFIG_CAPS;

typedef LONGLONG REFERENCE_TIME;

typedef interface IBaseFilter IBaseFilter;
typedef interface IReferenceClock IReferenceClock;

typedef enum _FilterState {
        State_Stopped,
        State_Paused,
        State_Running
} FILTER_STATE;

typedef enum _PinDirection {
        PINDIR_INPUT,
        PINDIR_OUTPUT
} PIN_DIRECTION;

#define MAX_PIN_NAME 128
typedef struct _PinInfo {
        IBaseFilter *pFilter;
        PIN_DIRECTION dir;
        WCHAR achName[MAX_PIN_NAME];
} PIN_INFO;

#define INTERFACE IPin
DECLARE_INTERFACE_(IPin,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(Connect)(THIS_ IPin*,const AM_MEDIA_TYPE*) PURE;
   STDMETHOD(ReceiveConnection)(THIS_ IPin*,const AM_MEDIA_TYPE*) PURE;
   STDMETHOD(Disconnect)(THIS) PURE;
   STDMETHOD(ConnectedTo)(THIS_  IPin*) PURE;
   STDMETHOD(ConnectionMediaType)(THIS_ AM_MEDIA_TYPE*) PURE;
   STDMETHOD(QueryPinInfo)(THIS_ PIN_INFO*) PURE;
   STDMETHOD(QueryDirection)(THIS_ PIN_DIRECTION*) PURE;
};
#undef INTERFACE

DECLARE_ENUMERATOR_(IEnumPins,IPin*);

typedef LONG_PTR OAEVENT;

#define INTERFACE IMediaEvent
DECLARE_INTERFACE_(IMediaEvent,IDispatch)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(GetEventHandle)(THIS_ OAEVENT*) PURE;
   STDMETHOD(GetEvent)(THIS_ long*,LONG_PTR,LONG_PTR,long) PURE;
   STDMETHOD(WaitForCompletion)(THIS_ long,long*) PURE;
   STDMETHOD(CancelDefaultHandling)(THIS_ long) PURE;
   STDMETHOD(RestoreDefaultHandling)(THIS_ long) PURE;
   STDMETHOD(FreeEventParams)(THIS_ long,LONG_PTR,LONG_PTR) PURE;
};
#undef INTERFACE

typedef long OAFilterState;

#define INTERFACE IMediaControl
DECLARE_INTERFACE_(IMediaControl,IDispatch)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(Run)(THIS) PURE;
   STDMETHOD(Pause)(THIS) PURE;
   STDMETHOD(Stop)(THIS) PURE;
   STDMETHOD(GetState)(THIS_ LONG,OAFilterState*) PURE;
   STDMETHOD(RenderFile)(THIS_ BSTR) PURE;
   STDMETHOD(AddSourceFilter)(THIS_ BSTR,IDispatch**) PURE;
   STDMETHOD(get_FilterCollection)(THIS_ IDispatch**) PURE;
   STDMETHOD(get_RegFilterCollection)(THIS_ IDispatch**) PURE;
   STDMETHOD(StopWhenReady)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE IVideoProcAmp
DECLARE_INTERFACE_(IVideoProcAmp,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE IAMStreamConfig
DECLARE_INTERFACE_(IAMStreamConfig,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(SetFormat)(THIS_ AM_MEDIA_TYPE*) PURE;
   STDMETHOD(GetFormat)(THIS_ AM_MEDIA_TYPE**) PURE;
   STDMETHOD(GetNumberOfCapabilities)(THIS_ int*,int*) PURE;
   STDMETHOD(GetStreamCaps)(THIS_ int,AM_MEDIA_TYPE**,BYTE*) PURE;
};
#undef INTERFACE

#define INTERFACE IMediaFilter
DECLARE_INTERFACE_(IMediaFilter,IPersist)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(Stop)(THIS) PURE;
   STDMETHOD(Pause)(THIS) PURE;
   STDMETHOD(Run)(THIS_ REFERENCE_TIME) PURE;
   STDMETHOD(GetState)(THIS_ DWORD,FILTER_STATE*) PURE;
   STDMETHOD(SetSyncSource)(THIS_ IReferenceClock*) PURE;
   STDMETHOD(GetSyncSource)(THIS_ IReferenceClock**) PURE;
};
#undef INTERFACE

#define INTERFACE IBaseFilter
DECLARE_INTERFACE_(IBaseFilter,IMediaFilter)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(EnumPins)(THIS_ IEnumPins**) PURE;
};
#undef INTERFACE

#define INTERFACE IEnumFilters
DECLARE_INTERFACE_(IEnumFilters,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(Next)(THIS_ ULONG,IBaseFilter**,ULONG*) PURE;
   STDMETHOD(Skip)(THIS_ ULONG) PURE;
   STDMETHOD(Reset)(THIS) PURE;
   STDMETHOD(Clone)(THIS_ IEnumFilters**) PURE;
};
#undef INTERFACE

#define INTERFACE IFilterGraph
DECLARE_INTERFACE_(IFilterGraph,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(AddFilter)(THIS_ IBaseFilter*,LPCWSTR) PURE;
   STDMETHOD(RemoveFilter)(THIS_ IBaseFilter*) PURE;
   STDMETHOD(EnumFilters)(THIS_ IEnumFilters**) PURE;
   STDMETHOD(FindFilterByName)(THIS_ LPCWSTR,IBaseFilter**) PURE;
   STDMETHOD(ConnectDirect)(THIS_ IPin*,IPin*,const AM_MEDIA_TYPE*) PURE;
   STDMETHOD(Reconnect)(THIS_ IPin*) PURE;
   STDMETHOD(Disconnect)(THIS_ IPin*) PURE;
   STDMETHOD(SetDefaultSyncSource)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE IGraphBuilder
DECLARE_INTERFACE_(IGraphBuilder,IFilterGraph)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(Connect)(THIS_ IPin*,IPin*) PURE;
   STDMETHOD(Render)(THIS_ IPin*) PURE;
   STDMETHOD(RenderFile)(THIS_ LPCWSTR,LPCWSTR) PURE;
   STDMETHOD(AddSourceFilter)(THIS_ LPCWSTR,LPCWSTR,IBaseFilter**) PURE;
   STDMETHOD(SetLogFile)(THIS_ DWORD_PTR) PURE;
   STDMETHOD(Abort)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE ICreateDevEnum
DECLARE_INTERFACE_(ICreateDevEnum,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(CreateClassEnumerator)(THIS_ REFIID,IEnumMoniker**,DWORD);
};
#undef INTERFACE

#define INTERFACE IMediaSample
DECLARE_INTERFACE_(IMediaSample,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
};
#undef INTERFACE

typedef struct {
    CLSID clsMedium;
    DWORD dw1;
    DWORD dw2;
} REGPINMEDIUM;

MIDL_INTERFACE("31EFAC30-515C-11d0-A9AA-00AA0061BE93")
IKsPropertySet : public IUnknown
{
public:
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Set(
        /* [in] */ REFGUID guidPropSet,
        /* [in] */ DWORD dwPropID,
        /* [size_is][in] */ LPVOID pInstanceData,
        /* [in] */ DWORD cbInstanceData,
        /* [size_is][in] */ LPVOID pPropData,
        /* [in] */ DWORD cbPropData) = 0;

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Get(
        /* [in] */ REFGUID guidPropSet,
        /* [in] */ DWORD dwPropID,
        /* [size_is][in] */ LPVOID pInstanceData,
        /* [in] */ DWORD cbInstanceData,
        /* [size_is][out] */ LPVOID pPropData,
        /* [in] */ DWORD cbPropData,
        /* [out] */ DWORD *pcbReturned) = 0;

    virtual HRESULT STDMETHODCALLTYPE QuerySupported(
        /* [in] */ REFGUID guidPropSet,
        /* [in] */ DWORD dwPropID,
        /* [out] */ DWORD *pTypeSupport) = 0;

};

#endif // ################################## strmif.h ##############################

#define INTERFACE ISampleGrabberCB
DECLARE_INTERFACE_(ISampleGrabberCB,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(SampleCB)(THIS_ double,IMediaSample*);
   STDMETHOD(BufferCB)(THIS_ double,BYTE*,long);
};
#undef INTERFACE

#define INTERFACE ISampleGrabber
DECLARE_INTERFACE_(ISampleGrabber,IUnknown)
{
   STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
   STDMETHOD_(ULONG,AddRef)(THIS) PURE;
   STDMETHOD_(ULONG,Release)(THIS) PURE;
   STDMETHOD(SetOneShot)(THIS_ BOOL);
   STDMETHOD(SetMediaType)(THIS_ const AM_MEDIA_TYPE*);
   STDMETHOD(GetConnectedMediaType)(THIS_ AM_MEDIA_TYPE*);
   STDMETHOD(SetBufferSamples)(THIS_ BOOL);
   STDMETHOD(GetCurrentBuffer)(THIS_ long*,long*);
   STDMETHOD(GetCurrentSample)(THIS_ IMediaSample**);
   STDMETHOD(SetCallBack)(THIS_ ISampleGrabberCB *,long);
};
#undef INTERFACE

////////////////////////////////////////////////////////////////////
// definitions for BDA support

// bdatypes.h
typedef struct _BDANODE_DESCRIPTOR
{
    ULONG ulBdaNodeType;
    GUID guidFunction;
    GUID guidName;
} BDANODE_DESCRIPTOR, *PBDANODE_DESCRIPTOR;

// bdatypes.h
typedef struct _BDA_TEMPLATE_CONNECTION
{
    ULONG FromNodeType;
    ULONG FromNodePinType;
    ULONG ToNodeType;
    ULONG ToNodePinType;
}BDA_TEMPLATE_CONNECTION, *PBDA_TEMPLATE_CONNECTION;

// bdaiface.h
MIDL_INTERFACE("79B56888-7FEA-4690-B45D-38FD3C7849BE")
IBDA_Topology : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetNodeTypes(
        /* [out][in] */ ULONG *pulcNodeTypes,
        /* [in] */ ULONG ulcNodeTypesMax,
        /* [size_is][out][in] */ ULONG rgulNodeTypes[  ]) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetNodeDescriptors(
        /* [out][in] */ ULONG *ulcNodeDescriptors,
        /* [in] */ ULONG ulcNodeDescriptorsMax,
        /* [size_is][out][in] */ BDANODE_DESCRIPTOR rgNodeDescriptors[  ]) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetNodeInterfaces(
        /* [in] */ ULONG ulNodeType,
        /* [out][in] */ ULONG *pulcInterfaces,
        /* [in] */ ULONG ulcInterfacesMax,
        /* [size_is][out][in] */ GUID rgguidInterfaces[  ]) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetPinTypes(
        /* [out][in] */ ULONG *pulcPinTypes,
        /* [in] */ ULONG ulcPinTypesMax,
        /* [size_is][out][in] */ ULONG rgulPinTypes[  ]) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetTemplateConnections(
        /* [out][in] */ ULONG *pulcConnections,
        /* [in] */ ULONG ulcConnectionsMax,
        /* [size_is][out][in] */ BDA_TEMPLATE_CONNECTION rgConnections[  ]) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreatePin(
        /* [in] */ ULONG ulPinType,
        /* [out][in] */ ULONG *pulPinId) = 0;

    virtual HRESULT STDMETHODCALLTYPE DeletePin(
        /* [in] */ ULONG ulPinId) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetMediaType(
        /* [in] */ ULONG ulPinId,
        /* [in] */ AM_MEDIA_TYPE *pMediaType) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetMedium(
        /* [in] */ ULONG ulPinId,
        /* [in] */ REGPINMEDIUM *pMedium) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateTopology(
        /* [in] */ ULONG ulInputPinId,
        /* [in] */ ULONG ulOutputPinId) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetControlNode(
        /* [in] */ ULONG ulInputPinId,
        /* [in] */ ULONG ulOutputPinId,
        /* [in] */ ULONG ulNodeType,
        /* [out][in] */ IUnknown **ppControlNode) = 0;

};

// bdatypes.h
typedef enum Polarisation {
    BDA_POLARISATION_NOT_SET = -1,
    BDA_POLARISATION_NOT_DEFINED = 0,
    BDA_POLARISATION_LINEAR_H = 1, // Linear horizontal polarisation
    BDA_POLARISATION_LINEAR_V, // Linear vertical polarisation
    BDA_POLARISATION_CIRCULAR_L, // Circular left polarisation
    BDA_POLARISATION_CIRCULAR_R, // Circular right polarisation
    BDA_POLARISATION_MAX,
} Polarisation;

// bdaiface.h
MIDL_INTERFACE("71985F47-1CA1-11d3-9CC8-00C04F7971E0")
IBDA_FrequencyFilter : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE put_Autotune(
        /* [in] */ ULONG ulTransponder) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Autotune(
        /* [out][in] */ ULONG *pulTransponder) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_Frequency(
        /* [in] */ ULONG ulFrequency) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Frequency(
        /* [out][in] */ ULONG *pulFrequency) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_Polarity(
        /* [in] */ Polarisation Polarity) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Polarity(
        /* [out][in] */ Polarisation *pPolarity) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_Range(
        /* [in] */ ULONG ulRange) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Range(
        /* [out][in] */ ULONG *pulRange) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_Bandwidth(
        /* [in] */ ULONG ulBandwidth) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Bandwidth(
        /* [out][in] */ ULONG *pulBandwidth) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_FrequencyMultiplier(
        /* [in] */ ULONG ulMultiplier) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_FrequencyMultiplier(
        /* [out][in] */ ULONG *pulMultiplier) = 0;

};

// bdemedia.h
typedef enum {
    KSPROPERTY_BDA_MODULATION_TYPE = 0,
    KSPROPERTY_BDA_INNER_FEC_TYPE,
    KSPROPERTY_BDA_INNER_FEC_RATE,
    KSPROPERTY_BDA_OUTER_FEC_TYPE,
    KSPROPERTY_BDA_OUTER_FEC_RATE,
    KSPROPERTY_BDA_SYMBOL_RATE,
    KSPROPERTY_BDA_SPECTRAL_INVERSION,
    KSPROPERTY_BDA_GUARD_INTERVAL,
    KSPROPERTY_BDA_TRANSMISSION_MODE,
    KSPROPERTY_BDA_ROLL_OFF,
    KSPROPERTY_BDA_PILOT,
    KSPROPERTY_BDA_SIGNALTIMEOUTS,
    KSPROPERTY_BDA_PLP_NUMBER
} KSPROPERTY_BDA_DIGITAL_DEMODULATOR;

// bdatypes.h
typedef enum ModulationType {
    BDA_MOD_NOT_SET = -1,
    BDA_MOD_NOT_DEFINED = 0,
    BDA_MOD_16QAM = 1,
    BDA_MOD_32QAM,
    BDA_MOD_64QAM,
    BDA_MOD_80QAM,
    BDA_MOD_96QAM,
    BDA_MOD_112QAM,
    BDA_MOD_128QAM,
    BDA_MOD_160QAM,
    BDA_MOD_192QAM,
    BDA_MOD_224QAM,
    BDA_MOD_256QAM,
    BDA_MOD_320QAM,
    BDA_MOD_384QAM,
    BDA_MOD_448QAM,
    BDA_MOD_512QAM,
    BDA_MOD_640QAM,
    BDA_MOD_768QAM,
    BDA_MOD_896QAM,
    BDA_MOD_1024QAM,
    BDA_MOD_QPSK,             // Quadrature Phase Shift Keying (including backwards compatible mode)
    BDA_MOD_BPSK,             // Binary Phase Shift Keying
    BDA_MOD_OQPSK,            // Offset QPSK
    BDA_MOD_8VSB,             // 8-Level Vestigial Sideband
    BDA_MOD_16VSB,            // 16-Level Vestigial Sideband
    BDA_MOD_ANALOG_AMPLITUDE, // std am
    BDA_MOD_ANALOG_FREQUENCY, // std fm
    BDA_MOD_8PSK,             // 8 Phase Shift Keying (including backwards compatible mode)
    BDA_MOD_RF, // analog TV (Video standards such as NTSC/PAL/SECAM specified in IAnalogLocator VideoStandard property)
    BDA_MOD_16APSK,           // DVB-S2 modulation 16-Level APSK
    BDA_MOD_32APSK,           // DVB-S2 modulation 32-Level APSK
    BDA_MOD_NBC_QPSK,         // Non-Backwards Compatible Quadrature Phase Shift Keying
    BDA_MOD_NBC_8PSK,         // Non-Backwards Compatible 8 Phase Shift Keying
    BDA_MOD_DIRECTV,          // DIRECTV DSS
    BDA_MOD_ISDB_T_TMCC,      // Automatic demodulation by Transmission and Multiplexing Configuration Control signal for ISDB-T
    BDA_MOD_ISDB_S_TMCC,      // Automatic demodulation by Transmission and Multiplexing Configuration Control signal for ISDB-S
    BDA_MOD_MAX,
} ModulationType;

// bdatypes.h
typedef enum FECMethod {
    BDA_FEC_METHOD_NOT_SET = -1,
    BDA_FEC_METHOD_NOT_DEFINED = 0,
    BDA_FEC_VITERBI = 1,    // FEC is a Viterbi Binary Convolution.
    BDA_FEC_RS_204_188,     // The FEC is Reed-Solomon 204/188 (outer FEC)
    BDA_FEC_LDPC,           // Low Density Parity Check error correction code
    BDA_FEC_BCH,            // Bose-Chaudhuri-Hocquenghem multiple error correction binary block code
    BDA_FEC_RS_147_130,     // The FEC is Reed-Solomon 147/130 (outer FEC) DirecTV-DSS
    BDA_FEC_MAX,
} FECMethod;

// bdatypes.h
typedef enum BinaryConvolutionCodeRate {
    BDA_BCC_RATE_NOT_SET = -1,
    BDA_BCC_RATE_NOT_DEFINED = 0,
    BDA_BCC_RATE_1_2 = 1,   // 1/2
    BDA_BCC_RATE_2_3,       // 2/3
    BDA_BCC_RATE_3_4,       // 3/4
    BDA_BCC_RATE_3_5,       // 3/5
    BDA_BCC_RATE_4_5,       // 4/5
    BDA_BCC_RATE_5_6,       // 5/6
    BDA_BCC_RATE_5_11,      // 5/11
    BDA_BCC_RATE_7_8,       // 7/8
    BDA_BCC_RATE_1_4,       // 1/4
    BDA_BCC_RATE_1_3,       // 1/3
    BDA_BCC_RATE_2_5,       // 2/5
    BDA_BCC_RATE_6_7,       // 6/7
    BDA_BCC_RATE_8_9,       // 8/9
    BDA_BCC_RATE_9_10,      // 9/10
    BDA_BCC_RATE_MAX,
} BinaryConvolutionCodeRate;

// bdatypes.h
typedef enum SpectralInversion {
    BDA_SPECTRAL_INVERSION_NOT_SET = -1,
    BDA_SPECTRAL_INVERSION_NOT_DEFINED = 0,
    BDA_SPECTRAL_INVERSION_AUTOMATIC = 1,
    BDA_SPECTRAL_INVERSION_NORMAL,
    BDA_SPECTRAL_INVERSION_INVERTED,
    BDA_SPECTRAL_INVERSION_MAX
} SpectralInversion;

// bdaiface.h
MIDL_INTERFACE("EF30F379-985B-4d10-B640-A79D5E04E1E0")
IBDA_DigitalDemodulator : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE put_ModulationType(
        /* [in] */ ModulationType *pModulationType) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_ModulationType(
        /* [out][in] */ ModulationType *pModulationType) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_InnerFECMethod(
        /* [in] */ FECMethod *pFECMethod) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_InnerFECMethod(
        /* [out][in] */ FECMethod *pFECMethod) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_InnerFECRate(
        /* [in] */ BinaryConvolutionCodeRate *pFECRate) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_InnerFECRate(
        /* [out][in] */ BinaryConvolutionCodeRate *pFECRate) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_OuterFECMethod(
        /* [in] */ FECMethod *pFECMethod) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_OuterFECMethod(
        /* [out][in] */ FECMethod *pFECMethod) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_OuterFECRate(
        /* [in] */ BinaryConvolutionCodeRate *pFECRate) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_OuterFECRate(
        /* [out][in] */ BinaryConvolutionCodeRate *pFECRate) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_SymbolRate(
        /* [in] */ ULONG *pSymbolRate) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_SymbolRate(
        /* [out][in] */ ULONG *pSymbolRate) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_SpectralInversion(
        /* [in] */ SpectralInversion *pSpectralInversion) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_SpectralInversion(
        /* [out][in] */ SpectralInversion *pSpectralInversion) = 0;

};

// bdatypes.h
typedef enum GuardInterval {
    BDA_GUARD_NOT_SET = -1,
    BDA_GUARD_NOT_DEFINED = 0,
    BDA_GUARD_1_32 = 1, // Guard interval is 1/32
    BDA_GUARD_1_16, // Guard interval is 1/16
    BDA_GUARD_1_8, // Guard interval is 1/8
    BDA_GUARD_1_4, // Guard interval is 1/4
    BDA_GUARD_1_128, // Guard interval is 1/128 (DVB-T2)
    BDA_GUARD_19_128, // Guard interval is 19/128 (DVB-T2)
    BDA_GUARD_19_256, // Guard interval is 19/256 (DVB-T2)
    BDA_GUARD_MAX,
} GuardInterval;

// bdatypes.h
typedef enum HierarchyAlpha {
    BDA_HALPHA_NOT_SET = -1,
    BDA_HALPHA_NOT_DEFINED = 0,
    BDA_HALPHA_1 = 1, // Hierarchy alpha is 1.
    BDA_HALPHA_2, // Hierarchy alpha is 2.
    BDA_HALPHA_4, // Hierarchy alpha is 4.
    BDA_HALPHA_MAX,
} HierarchyAlpha;

// bdatypes.h
typedef enum TransmissionMode {
    BDA_XMIT_MODE_NOT_SET = -1,
    BDA_XMIT_MODE_NOT_DEFINED = 0,
    BDA_XMIT_MODE_2K = 1, // Transmission uses 1705 carriers (use a 2K FFT)
    BDA_XMIT_MODE_8K,     // Transmission uses 6817 carriers (use an 8K FFT)
    BDA_XMIT_MODE_4K,
    BDA_XMIT_MODE_2K_INTERLEAVED,
    BDA_XMIT_MODE_4K_INTERLEAVED,
    BDA_XMIT_MODE_1K,    //DVB-T2 (use 1K FFT)
    BDA_XMIT_MODE_16K,   //DVB-T2 (use 16K FFT)
    BDA_XMIT_MODE_32K,   //DVB-T2 (use 32K FFT)
    BDA_XMIT_MODE_MAX,
} TransmissionMode;

// bdatypes.h
typedef enum RollOff {
    BDA_ROLL_OFF_NOT_SET = -1,
    BDA_ROLL_OFF_NOT_DEFINED = 0,
    BDA_ROLL_OFF_20 = 1,         // .20 Roll Off (DVB-S2 Only)
    BDA_ROLL_OFF_25,             // .25 Roll Off (DVB-S2 Only)
    BDA_ROLL_OFF_35,             // .35 Roll Off (DVB-S2 Only)
    BDA_ROLL_OFF_MAX,
} RollOff;

// bdatypes.h
typedef enum Pilot {
    BDA_PILOT_NOT_SET = -1,
    BDA_PILOT_NOT_DEFINED = 0,
    BDA_PILOT_OFF = 1,           // Pilot Off (DVB-S2 Only)
    BDA_PILOT_ON,                // Pilot On  (DVB-S2 Only)
    BDA_PILOT_MAX,
} Pilot;

// bdaiface.h
MIDL_INTERFACE("525ED3EE-5CF3-4e1e-9A06-5368A84F9A6E")
IBDA_DigitalDemodulator2 : public IBDA_DigitalDemodulator
{
public:
    virtual HRESULT STDMETHODCALLTYPE put_GuardInterval(
        /* [in] */ GuardInterval *pGuardInterval) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_GuardInterval(
        /* [out][in] */ GuardInterval *pGuardInterval) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_TransmissionMode(
        /* [in] */ TransmissionMode *pTransmissionMode) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_TransmissionMode(
        /* [out][in] */ TransmissionMode *pTransmissionMode) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_RollOff(
        /* [in] */ RollOff *pRollOff) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_RollOff(
        /* [out][in] */ RollOff *pRollOff) = 0;

    virtual HRESULT STDMETHODCALLTYPE put_Pilot(
        /* [in] */ Pilot *pPilot) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Pilot(
        /* [out][in] */ Pilot *pPilot) = 0;

};

// Ks.h
struct KSPROPERTY {
  GUID  Set;
  ULONG Id;
  ULONG Flags;
};

///////////////////////////////////////////////////////////////////////////////

string GUID2String(GUID pGUID)
{
    string tResult = "";

    char tBuffer[512];
    snprintf(tBuffer, 512, "{%08x-%04x-%04x-%02x%02x-%02x-%02x-%02x-%02x-%02x-%02x}",
            pGUID.Data1, pGUID.Data2, pGUID.Data3,
            pGUID.Data4[0], pGUID.Data4[1], pGUID.Data4[2], pGUID.Data4[3],
            pGUID.Data4[4], pGUID.Data4[5], pGUID.Data4[6], pGUID.Data4[7]);

    tResult = string(tBuffer);

    return tResult;
}

string GetMediaTypeName(GUID pType)
{
    string tResult = "";

    if (pType == MEDIATYPE_Video)
    {
        tResult = "Video";
    }else if (pType == MEDIATYPE_Audio)
    {
        tResult = "Audio";
    }else if (pType == MEDIATYPE_Stream)
    {
        tResult = "Stream";
    }else if (pType == MEDIATYPE_VBI)
    {
        tResult = "Video vertical blinking interval";
    }else if (pType == MEDIATYPE_Interleaved)
    {
        tResult = "Interleaved";
    }else
    {
        tResult = GUID2String(pType);
    }

    return tResult;
}

string GetSubTypeName(GUID pType)
{
    string tResult = "";

    if (pType == MEDIASUBTYPE_MPEG2_TRANSPORT)
    {
        tResult = "Mpeg2 transport";
    }else

    if (pType == MEDIASUBTYPE_AYUV)
    {
        tResult = "AYUV 4:4:4 Packed 8";
    }else if (pType == MEDIASUBTYPE_YUY2)
    {
        tResult = "YUY2 4:2:2 Packed 8";
    }else if (pType == MEDIASUBTYPE_UYVY)
    {
        tResult = "UYVY 4:2:2 Packed 8";
    }else if (pType == MEDIASUBTYPE_YV12)
    {
        tResult = "YV12 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_NV12)
    {
        tResult = "NV12 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_I420)
    {
        tResult = "I420 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_IF09)
    {
        tResult = "Indeo YVU9 Planar 8";
    }else if (pType == MEDIASUBTYPE_IYUV)
    {
        tResult = "IYUV 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_Y211)
    {
        tResult = "Y211 Packed 8";
    }else if (pType == MEDIASUBTYPE_Y411)
    {
        tResult = "Y411 4:1:1 Packed 8";
    }else if (pType == MEDIASUBTYPE_Y41P)
    {
        tResult = "Y41P 4:1:1 Packed 8";
    }else if (pType == MEDIASUBTYPE_YVU9)
    {
        tResult = "YVU9    Planar 8";
    }else if (pType == MEDIASUBTYPE_YVYU)
    {
        tResult = "YVYU 4:2:2 Packed 8";
    }else if (pType == MEDIASUBTYPE_RGB24)
    {
        tResult = "RGB 24 bpp";
    }else if (pType == MEDIASUBTYPE_Y8)
    {
        tResult = "Y8 monochrome";
    }else if (pType == MEDIASUBTYPE_Y800)
    {
        tResult = "Y800 monochrome";
    }else if (pType == MEDIASUBTYPE_GREY)
    {
        tResult = "GREY monochrome";
    }else if (pType == MEDIASUBTYPE_IMC1)
    {
        tResult = "IMC1 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_IMC2)
    {
        tResult = "IMC2 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_IMC3)
    {
        tResult = "IMC3 4:2:0 Planar 8";
    }else if (pType == MEDIASUBTYPE_IMC4)
    {
        tResult = "IMC4 4:2:0 Planar 8";
    }else
    {
        tResult = GUID2String(pType);
    }
    return tResult;
}

#endif
#endif
