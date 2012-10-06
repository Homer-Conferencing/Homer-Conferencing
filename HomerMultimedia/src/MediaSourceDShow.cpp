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
 * Purpose: Implementation of a DirectShow based local video source
 * Author:  Thomas Volkert
 * Since:   2012-08-16
 */

// the following source was inspired by:
//		"PlayCap" example from the Microsoft Windows SDK
//		http://msdn.microsoft.com/en-us/library/windows/desktop/dd407292%28v=vs.85%29.aspx
//		http://msdn.microsoft.com/en-us/library/windows/desktop/dd377566%28v=vs.85%29.aspx
// 		http://msdn.microsoft.com/en-us/library/windows/desktop/dd375620%28v=vs.85%29.aspx
//		http://sourceforge.net/tracker/?func=detail&atid=302435&aid=1819367&group_id=2435 - "Jan Wedekind"

#include <MediaSourceDShow.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <Header_Ffmpeg.h>

#include <cstdio>
#include <string.h>
#include <stdio.h>

#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <objidl.h>

#define COM_NO_WINDOWS_H
#include <oaidl.h>
#include <ocidl.h>
#include <winnls.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

DEFINE_GUID( CLSID_VideoInputDeviceCategory, 0x860BB310, 0x5D01,
             0x11d0, 0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86);
DEFINE_GUID( CLSID_SystemDeviceEnum, 0x62BE5D10, 0x60EB, 0x11d0,
             0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86 );
DEFINE_GUID( CLSID_FilterGraph, 0xe436ebb3, 0x524f, 0x11ce,
             0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3,
             0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 );
DEFINE_GUID( CLSID_NullRenderer,0xc1f400a4, 0x3f08, 0x11d3,
             0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 );
DEFINE_GUID( CLSID_VfwCapture, 0x1b544c22, 0xfd0b, 0x11ce,
             0x8c, 0x63, 0x0, 0xaa, 0x00, 0x44, 0xb5, 0x1e);
DEFINE_GUID( IID_IGraphBuilder, 0x56a868a9, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IBaseFilter, 0x56a86895, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0,
             0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86 );
DEFINE_GUID( IID_IEnumFilters, 0x56a86893, 0xad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_IMediaSample, 0x56a8689a, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce,
             0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f );
DEFINE_GUID( IID_IMediaEvent, 0x56a868b6, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_IMediaControl, 0x56a868b1, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_IAMStreamConfig, 0xc6e13340, 0x30ac, 0x11d0,
             0xa1, 0x8c, 0x00, 0xa0, 0xc9, 0x11, 0x89, 0x56 );
DEFINE_GUID( IID_IVideoProcAmp, 0x4050560e, 0x42a7, 0x413a,
             0x85, 0xc2, 0x09, 0x26, 0x9a, 0x2d, 0x0f, 0x44 );
DEFINE_GUID( MEDIATYPE_Video, 0x73646976, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YV12, 0x32315659, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( MEDIASUBTYPE_NV12, 0x3231564E, 0x0000, 0x0010,
		 	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IF09, 0x39304649, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IYUV, 0x56555949, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( MEDIASUBTYPE_YUYV, 0x56595559, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( MEDIASUBTYPE_AYUV, 0x56555941, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YUY2, 0x32595559, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( MEDIASUBTYPE_UYVY, 0x59565955, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( MEDIASUBTYPE_RGB24, 0xe436eb7d, 0x524f, 0x11ce,
             0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( MEDIASUBTYPE_Y211, 0x31313259, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y411, 0x31313459, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y41P, 0x50313459, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YVU9, 0x39555659, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_YVYU, 0x55595659, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_GREY, 0x59455247, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y8, 0x20203859, 0x0000, 0x0010,
		 	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_Y800, 0x30303859, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIASUBTYPE_IMC1, 0x31434d49, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIATYPE_Interleaved, 0x73766169, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010,
			 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
DEFINE_GUID( MEDIATYPE_VBI, 0xf72a76e1, 0xeb0a, 0x11d0,
			 0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID( FORMAT_VideoInfo, 0x05589f80, 0xc356, 0x11ce,
			 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID( FORMAT_VideoInfo2, 0xf72a76a0, 0xeb0a, 0x11d0,
			 0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);

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

///////////////////////////////////////////////////////////////////////////////

MediaSourceDShow::MediaSourceDShow(string pDesiredDevice):
    MediaSource("DShow: local capture")
{
    mSourceType = SOURCE_DEVICE;
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new vfw device when creating source object");
    }
    LOG(LOG_VERBOSE, "Created");
}

MediaSourceDShow::~MediaSourceDShow()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
    LOG(LOG_VERBOSE, "Destroyed");
}

bool MediaSourceDShow::SupportsDecoderFrameStatistics()
{
    return (mMediaType == MEDIA_VIDEO);
}

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
		tResult = "YVU9	Planar 8";
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
	}else
	{
		tResult = GUID2String(pType);
	}

/*
//		case MEDIASUBTYPE_IMC3:
//			tResult = "IMC2 4:2:0 Planar 8";
//		case MEDIASUBTYPE_IMC2:
//			tResult = "IMC3 4:2:0 Planar 8";
//		case MEDIASUBTYPE_IMC4:
//			tResult = "IMC4 4:2:0 Planar 8";
*/
	return tResult;
}

void MediaSourceDShow::getVideoDevices(VideoDevices &pVList)
{
    static bool 			tFirstCall = true;
    VARIANT 				tVariant;
    TCHAR 					tDeviceName[256];
    TCHAR 					tDevicePath[256];
    TCHAR 					tDeviceDescription[256];
    VideoDeviceDescriptor 	tDevice;
    HRESULT 				tRes = S_OK;
    IMoniker				*tMoniker =NULL;
    ICreateDevEnum 			*tDeviceEnumerator =NULL;
    IEnumMoniker 			*tClassEnumerator = NULL;
    CLSID 					tClassId;
    bool					tDeviceIsVFW;
    IBaseFilter				*tSource;
	IPropertyBag 			*tPropertyBag;
	IAMStreamConfig 		*tStreamConfig = NULL;
	IEnumPins  				*tPinsEnum = NULL;
	IPin					*tPin = NULL;
	PIN_DIRECTION			tPinDir;

	#ifdef MSDS_DEBUG_PACKETS
		tFirstCall = true;
	#endif

	if (tFirstCall)
		LOG(LOG_VERBOSE, "Enumerating hardware..");

    VariantInit(&tVariant);

    // create system device enumerator
    tRes = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &tDeviceEnumerator);
    if (FAILED(tRes))
    {
    	LOG(LOG_ERROR, "Could not create the Windows system device enumerator");
    	return;
    }

    // create enumerator capture devices
    tRes = tDeviceEnumerator->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &tClassEnumerator, 0);
	if (FAILED(tRes))
	{
    	LOG(LOG_ERROR, "Could not create the Windows enumerator for video input devices");
    	return;
    }

	if (tClassEnumerator == NULL)
	{
		LOG(LOG_VERBOSE, "No DirectShow video capture device was detected");
		return;
	}

	int tCount = -1;
	// access video input device
	while(tClassEnumerator->Next (1, &tMoniker, NULL) == S_OK)
	{
		memset(tDeviceName, 0, sizeof(tDeviceName));
		memset(tDeviceDescription, 0, sizeof(tDeviceDescription));
		memset(tDevicePath, 0, sizeof(tDevicePath));
		tCount++;

		// bind to storage
		tRes = tMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&tPropertyBag);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not bind moniker to storage object");
			continue;
		}

		// retrieve the device's name
		tRes = tPropertyBag->Read(L"FriendlyName", &tVariant, 0);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not get property \"FriendlyName\" for device \"%d\"", tCount);
			tPropertyBag->Release();
			continue;
		}
		WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDeviceName, sizeof(tDeviceName), 0, 0);
		VariantClear(&tVariant);

		// retrieve the device's description
		tRes = tPropertyBag->Read(L"Description", &tVariant, 0);
		if (SUCCEEDED(tRes))
			WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDeviceDescription, sizeof(tDeviceDescription), 0, 0);
		else
			LOG(LOG_WARN, "Could not get property \"Description\" for device \"%d\"", tCount);
		VariantClear(&tVariant);

		// retrieve the device's description
		tRes = tPropertyBag->Read(L"DevicePath", &tVariant, 0);
		if (SUCCEEDED(tRes))
			WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDevicePath, sizeof(tDevicePath), 0, 0);
		else
			LOG(LOG_WARN, "Could not get property \"DevicePath\" for device \"%d\"", tCount);
		VariantClear(&tVariant);


		// bint to object
		string tDeviceNameStr = string(tDeviceName);
		LOG(LOG_VERBOSE, "Will try to detect supported video resolutions for device \"%s\"", tDeviceNameStr.c_str());
		tMoniker->BindToObject( 0, 0, IID_IBaseFilter, (void **)&tSource);

	    tRes = tSource->GetClassID(&tClassId);
	    if (tClassId == CLSID_VfwCapture)
	    	tDeviceIsVFW = true;
	    else
	    	tDeviceIsVFW = false;

		//####################################
		//### verbose output and store device description
		//####################################
		tDevice.Name = string(tDeviceName);
		tDevice.Card = "video=" + string(tDeviceName);
		tDevice.Desc = "DirectShow based video device \"" + string(tDeviceName) + "\"";
		tDevice.Type = Camera; // assume all as camera devices

        if (tFirstCall)
        {
			LOG(LOG_INFO, "Found active DirectShow device %d", tCount);
			LOG(LOG_INFO, "  ..device name: %s", tDeviceName);
			LOG(LOG_INFO, "  ..description: %s", tDeviceDescription);
			LOG(LOG_INFO, "  ..path: %s", tDevicePath);
			LOG(LOG_INFO, "  ..type: %s", tDeviceIsVFW ? "VFW device" : "DirectShow device");
        }

        // enumerate all supported video resolutions
        if (tFirstCall)
        {
			tRes = tSource->EnumPins(&tPinsEnum);
			if (FAILED(tRes))
			{
				LOG(LOG_ERROR, "Could not enumerate pins");
				tPropertyBag->Release();
				break;
			}

			while (tPinsEnum->Next(1, &tPin, 0) == S_OK)
			{
				tRes = tPin->QueryDirection(&tPinDir);
				if (FAILED(tRes))
				{
					LOG(LOG_ERROR, "Could not query pin direction");
					break;
				}

				if (tPinDir == PINDIR_OUTPUT)
				{
					tRes = tPin->QueryInterface(IID_IAMStreamConfig, (void **)&tStreamConfig);
					if (FAILED(tRes))
					{
						LOG(LOG_ERROR, "Could not query pin interface");
						tPin->Release();
						break;
					}

					int tCount, tSize;
					tRes = tStreamConfig->GetNumberOfCapabilities( &tCount, &tSize);
					LOG(LOG_VERBOSE, "Found %d capability entries", tCount);
					if (FAILED(tRes))
					{
						LOG(LOG_ERROR, "Could not get number of caps");
						tPin->Release();
						break;
					}

			    	AM_MEDIA_TYPE *tMT;
			    	BYTE *tCaps = new BYTE[tSize]; // VIDEO_STREAM_CONFIG_CAPS

			    	for ( int i = 0; i < tCount; i++)
				    {
			    		tRes = tStreamConfig->GetStreamCaps(i, &tMT, tCaps);
						if (FAILED(tRes))
						{
							#ifdef MSDS_DEBUG_RESOLUTIONS
								LOG(LOG_ERROR, "Could not get stream cap %d", i);
							#endif
							break;
						}

					    if ((tMT->majortype == MEDIATYPE_Video) && (tMT->pbFormat !=0))
					    {
					    	int tFormatType = -1;
							int tWidth = -1;
							int tHeight = -1;
					    	if (tMT->formattype == FORMAT_VideoInfo)
					    	{// VideoInfo
					    		VIDEOINFOHEADER *tVInfoHeader = (VIDEOINFOHEADER*)tMT->pbFormat;
								tWidth = tVInfoHeader->bmiHeader.biWidth;
								tHeight = tVInfoHeader->bmiHeader.biHeight;
								tFormatType = 1;
					    	}else if (tMT->formattype == FORMAT_VideoInfo2)
					    	{// VideoInfo2
					    		VIDEOINFOHEADER2 *tVInfoHeader2 = (VIDEOINFOHEADER2*)tMT->pbFormat;
								tWidth = tVInfoHeader2->bmiHeader.biWidth;
								tHeight = tVInfoHeader2->bmiHeader.biHeight;
								tFormatType = 2;
					    	}else
					    	{
					    		LOG(LOG_WARN, "Unsupported format type detected: %s", GUID2String(tMT->formattype).c_str());
					    	}

							LOG(LOG_VERBOSE, "  ..supported media tyoe: Video, video info format: %d, video resolution: %d*%d (sub type: %s)", tFormatType, tWidth, tHeight, GetSubTypeName(tMT->subtype).c_str());
						}else
						{
							LOG(LOG_VERBOSE, "  ..additional media type: %s", GetMediaTypeName(tMT->majortype).c_str());
						}
				    }
			    	delete [] tCaps;
				}
				tPin->Release();
			}
			tPinsEnum->Release();
        }

        //###############################################
		//### finally add this device to the result list
		//###############################################
		pVList.push_back(tDevice);

		tPropertyBag->Release();
		tMoniker->Release();
	}
	tClassEnumerator->Release();

    tFirstCall = false;
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceDShow::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    int                 tResult;
    AVDictionary        *tOptions = NULL;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;

    mMediaType = MEDIA_VIDEO;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(DShow)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    av_dict_set(&tOptions, "video_size", (toString(pResX) + "x" + toString(pResY)).c_str(), 0);
    av_dict_set(&tOptions, "framerate", toString((int)pFps).c_str(), 0);

    tFormat = av_find_input_format("dshow");
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find input format");
        return false;
    }

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = "";

    // alocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    LOG(LOG_VERBOSE, "Going to open input device \"%s\" with resolution %d*%d..", mDesiredDevice.c_str(), pResX, pResY);
    if (mDesiredDevice != "")
    {
        //########################################
        //### probing given device file
        //########################################
        if ((tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions)) != 0)
        {
            if (tResult != 0)
            {
				LOG(LOG_WARN, "Device open failed - switching back to default values");

				av_dict_free(&tOptions);
				tOptions = NULL;

				//av_dict_set(&tOptions, "video_size", "352x288", 0);
				tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions);
            }
            if (tResult != 0)
            {
        		LOG(LOG_ERROR, "Couldn't open device \"%s\" because of \"%s\".", mDesiredDevice.c_str(), strerror(AVUNERROR(tResult)));
        		return false;
            }
        }
    }else
    {
        //########################################
        //### auto probing possible device files
        //########################################
        LOG(LOG_VERBOSE, "Auto-probing for DShow capture device");
        VideoDevices tDevices;
        getVideoDevices(tDevices);

        bool tFound = false;
        for (int i = 0; i < (int)tDevices.size(); i++)
        {
			LOG(LOG_VERBOSE, "Probing DShow device number: %d", i);
			mDesiredDevice = tDevices[i].Card;
			if ((tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions)) == 0)
			{
				LOG(LOG_VERBOSE, " ..available device connected");
				tFound = true;
				break;
			}
        }
        if (!tFound)
        {
            LOG(LOG_INFO, "Couldn't find a fitting DShow video device");
            return false;
        }
    }

    av_dict_free(&tOptions);

    mCurrentDevice = mDesiredDevice;

    if (!DetectAllStreams())
    	return false;

    if (!SelectStream())
    	return false;

    mFormatContext->streams[mMediaStreamIndex]->time_base.num = 100;
    mFormatContext->streams[mMediaStreamIndex]->time_base.den = (int)pFps * 100;

    if (!OpenDecoder())
    	return false;

    //######################################################
    //### create context for picture scaler
    //######################################################
    mScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    //###########################################################################################
    //### seek to the current position and drop data received during codec auto detection phase
    //##########################################################################################
    //av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    // Allocate video frame for source and RGB format
    if ((mSourceFrame = avcodec_alloc_frame()) == NULL)
        return false;
    if ((mRGBFrame = avcodec_alloc_frame()) == NULL)
        return false;

    MarkOpenGrabDeviceSuccessful();

    mDecodedIFrames = 0;
    mDecodedPFrames = 0;
    mDecodedBFrames = 0;

    return true;
}

bool MediaSourceDShow::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceDShow::CloseGrabDevice()
{
	bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type");
        return false;
    }

    if (mMediaSourceOpened)
    {
        StopRecording();

        mMediaSourceOpened = false;

        // free the software scaler context
        sws_freeContext(mScalerContext);

        // Close the DShow codec
        avcodec_close(mCodecContext);

		// Close the DShow video file
        HM_close_input(mFormatContext);

        // Free the frames
        av_free(mRGBFrame);
        av_free(mSourceFrame);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceDShow::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    AVPacket            tPacket;
    int                 tFrameFinished = 0;
    int                 tBytesDecoded = 0;

    // lock grabbing
    mGrabMutex.lock();

    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("grab buffer is NULL");

        return GRAB_RES_INVALID;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("video source is closed");

        return GRAB_RES_INVALID;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("video source is paused");

        return GRAB_RES_INVALID;
    }

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)mRGBFrame, (uint8_t *)pChunkBuffer, PIX_FMT_RGB32, mTargetResX, mTargetResY);

    // Read new packet
    // return 0 if OK, < 0 if error or end of file.
    do
    {
        // read next frame from video source - blocking
        if (av_read_frame(mFormatContext, &tPacket) != 0)
        {
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge failed
            MarkGrabChunkFailed("could'nt read next video frame");

            return GRAB_RES_INVALID;
        }
    }while (tPacket.stream_index != mMediaStreamIndex);

    if ((tPacket.data != NULL) && (tPacket.size > 0))
    {
        #ifdef MSDS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed new video packet:");
            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
            LOG(LOG_VERBOSE, "      ..pts: %ld stream [%d] pts: %ld", tPacket.pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts);
            LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
            LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
        #endif

        // log statistics about original packets from device
        AnnouncePacket(tPacket.size);

        // decode packet and get a frame if the new frame is of any use for us
        if ((!pDropChunk) || (mRecording))
        {
            // Decode the next chunk of data
            tBytesDecoded = HM_avcodec_decode_video(mCodecContext, mSourceFrame, &tFrameFinished, &tPacket);

            // emulate set FPS
            mSourceFrame->pts = FpsEmulationGetPts();

//        // transfer the presentation time value
//        mSourceFrame->pts = tPacket.pts;

            #ifdef MSDS_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Source video frame..");
                LOG(LOG_VERBOSE, "      ..key frame: %d", mSourceFrame->key_frame);
                switch(mSourceFrame->pict_type)
                {
                        case AV_PICTURE_TYPE_I:
                            LOG(LOG_VERBOSE, "      ..picture type: i-frame");
                            break;
                        case AV_PICTURE_TYPE_P:
                            LOG(LOG_VERBOSE, "      ..picture type: p-frame");
                            break;
                        case AV_PICTURE_TYPE_B:
                            LOG(LOG_VERBOSE, "      ..picture type: b-frame");
                            break;
                        default:
                            LOG(LOG_VERBOSE, "      ..picture type: %d", mSourceFrame->pict_type);
                            break;
                }
                LOG(LOG_VERBOSE, "      ..pts: %ld", mSourceFrame->pts);
                LOG(LOG_VERBOSE, "      ..coded pic number: %d", mSourceFrame->coded_picture_number);
                LOG(LOG_VERBOSE, "      ..display pic number: %d", mSourceFrame->display_picture_number);
            #endif

			if ((tFrameFinished) && (tBytesDecoded > 0))
			{
                // ############################
                // ### ANNOUNCE FRAME (statistics)
                // ############################
                AnnounceFrame(mSourceFrame);

                // ############################
                // ### RECORD FRAME
                // ############################
                if (mRecording)
                    RecordFrame(mSourceFrame);

                // ############################
                // ### SCALE FRAME (CONVERT)
                // ############################
				if (!pDropChunk)
				{
					HM_sws_scale(mScalerContext, mSourceFrame->data, mSourceFrame->linesize, 0, mCodecContext->height, mRGBFrame->data, mRGBFrame->linesize);
				}
			}else
			{
				// unlock grabbing
				mGrabMutex.unlock();

				// acknowledge failed
				MarkGrabChunkFailed("couldn't decode video frame");

				return GRAB_RES_INVALID;
			}
        }
        av_free_packet(&tPacket);
    }

    // return size of decoded frame
    pChunkSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) * sizeof(uint8_t);

    // unlock grabbing
    mGrabMutex.unlock();

    mChunkNumber++;

    // acknowledge success
    MarkGrabChunkSuccessful(mChunkNumber);

    return mChunkNumber;
}

GrabResolutions MediaSourceDShow::GetSupportedVideoGrabResolutions()
{
    static bool 			tFirstCall = true;
    VARIANT 				tVariant;
    TCHAR 					tDeviceName[256];
    TCHAR 					tDevicePath[256];
    TCHAR 					tDeviceDescription[256];
    VideoDeviceDescriptor 	tDevice;
    HRESULT 				tRes = S_OK;
    IMoniker				*tMoniker =NULL;
    ICreateDevEnum 			*tDeviceEnumerator =NULL;
    IEnumMoniker 			*tClassEnumerator = NULL;
    CLSID 					tClassId;
    bool					tDeviceIsVFW;
    IBaseFilter				*tSource;
	IPropertyBag 			*tPropertyBag;
	VideoFormatDescriptor 	tFormat;
	IAMStreamConfig 		*tStreamConfig = NULL;
	IEnumPins  				*tPinsEnum = NULL;
	IPin					*tPin = NULL;
	PIN_DIRECTION			tPinDir;

    if (mMediaType != MEDIA_VIDEO)
    {
    	LOG(LOG_ERROR, "Invalid media type");
		return mSupportedVideoFormats;
    }

    if (mCurrentDeviceName == "")
    {
    	LOG(LOG_ERROR, "Current device name is empty");
		return mSupportedVideoFormats;
    }

    mSupportedVideoFormats.clear();

    VariantInit(&tVariant);

    // create system device enumerator
    tRes = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &tDeviceEnumerator);
    if (FAILED(tRes))
    {
    	LOG(LOG_ERROR, "Could not create the Windows system device enumerator");
    	return mSupportedVideoFormats;
    }

    // create enumerator capture devices
    tRes = tDeviceEnumerator->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &tClassEnumerator, 0);
	if (FAILED(tRes))
	{
    	LOG(LOG_ERROR, "Could not create the Windows enumerator for video input devices");
    	return mSupportedVideoFormats;
    }

	if (tClassEnumerator == NULL)
	{
		LOG(LOG_VERBOSE, "No DirectShow video capture device was detected");
		return mSupportedVideoFormats;
	}

	int tCount = -1;
	// access video input device
	while(tClassEnumerator->Next (1, &tMoniker, NULL) == S_OK)
	{
		memset(tDeviceName, 0, sizeof(tDeviceName));
		memset(tDeviceDescription, 0, sizeof(tDeviceDescription));
		memset(tDevicePath, 0, sizeof(tDevicePath));
		tCount++;

		// bind to storage
		tRes = tMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&tPropertyBag);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not bind moniker to storage object");
			continue;
		}

		// retrieve the device's name
		tRes = tPropertyBag->Read(L"FriendlyName", &tVariant, 0);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not get property \"FriendlyName\" for device %d", tCount);
			tPropertyBag->Release();
			continue;
		}
		WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDeviceName, sizeof(tDeviceName), 0, 0);
		VariantClear(&tVariant);

		// have we found our current device in the enumeration?
		string tDeviceNameStr = string(tDeviceName);
		LOG(LOG_VERBOSE, "Comparing %s and %s for video resolution detection", tDeviceNameStr.c_str(), mCurrentDeviceName.c_str());
		if (tDeviceNameStr == mCurrentDeviceName)
		{
			LOG(LOG_VERBOSE, "Will try to detect supported video resolutions for device %s", tDeviceNameStr.c_str());
			// bint to object
			tMoniker->BindToObject( 0, 0, IID_IBaseFilter, (void **)&tSource);

			tRes = tSource->EnumPins(&tPinsEnum);
			if (FAILED(tRes))
			{
				LOG(LOG_ERROR, "Could not enumerate pins");
				tPropertyBag->Release();
				break;
			}

			while (tPinsEnum->Next(1, &tPin, 0) == S_OK)
			{
				tRes = tPin->QueryDirection(&tPinDir);
				if (FAILED(tRes))
				{
					LOG(LOG_ERROR, "Could not query pin direction");
					break;
				}

				if (tPinDir == PINDIR_OUTPUT)
				{
					tRes = tPin->QueryInterface(IID_IAMStreamConfig, (void **)&tStreamConfig);
					if (FAILED(tRes))
					{
						LOG(LOG_ERROR, "Could not query pin interface");
						tPin->Release();
						break;
					}

					int tCount, tSize;
					tRes = tStreamConfig->GetNumberOfCapabilities( &tCount, &tSize);
					LOG(LOG_VERBOSE, "Found %d capability entries", tCount);
					if (FAILED(tRes))
					{
						LOG(LOG_ERROR, "Could not get number of caps");
						tPin->Release();
						break;
					}

			    	AM_MEDIA_TYPE *tMT;
			    	BYTE *tCaps = new BYTE[tSize]; // VIDEO_STREAM_CONFIG_CAPS

			    	for ( int i = 0; i < tCount; i++)
				    {
			    		tRes = tStreamConfig->GetStreamCaps(i, &tMT, tCaps);
						if (FAILED(tRes))
						{
							#ifdef MSDS_DEBUG_RESOLUTIONS
								LOG(LOG_ERROR, "Could not get stream cap %d", i);
							#endif
							break;
						}

					    if ((tMT->majortype == MEDIATYPE_Video) && (tMT->pbFormat !=0))
					    {
							int tWidth = -1;
							int tHeight = -1;
					    	if (tMT->formattype == FORMAT_VideoInfo)
					    	{// VideoInfo
					    		VIDEOINFOHEADER *tVInfoHeader = (VIDEOINFOHEADER*)tMT->pbFormat;
								tWidth = tVInfoHeader->bmiHeader.biWidth;
								tHeight = tVInfoHeader->bmiHeader.biHeight;
					    	}else if (tMT->formattype == FORMAT_VideoInfo2)
					    	{// VideoInfo2
					    		VIDEOINFOHEADER2 *tVInfoHeader2 = (VIDEOINFOHEADER2*)tMT->pbFormat;
								tWidth = tVInfoHeader2->bmiHeader.biWidth;
								tHeight = tVInfoHeader2->bmiHeader.biHeight;
					    	}else
					    	{
					    		LOG(LOG_WARN, "Unsupported format type detected: %s", GUID2String(tMT->formattype).c_str());
					    	}

							#ifdef MSDS_DEBUG_RESOLUTIONS
								LOG(LOG_VERBOSE, "Detected source video resolution %d*%d and sub type: %s", tWidth, tHeight, GetSubTypeName(tMT->subtype).c_str());
							#endif
							if ((tWidth <= 1920) && (tHeight <= 1080))
							{
								tFormat.Name= "";
								tFormat.ResX = tWidth;
								tFormat.ResY = tHeight;
								bool tAlreadyKnownResolution = false;
								for (int i = 0; i < (int)mSupportedVideoFormats.size(); i++)
								{
									if ((mSupportedVideoFormats[i].ResX == tWidth) && (mSupportedVideoFormats[i].ResY == tHeight))
									{
										tAlreadyKnownResolution = true;
										break;
									}
								}
								if (!tAlreadyKnownResolution)
								{
		                            if ((tHeight > 0) && (tWidth > 0))
		                            {
		                                LOG(LOG_VERBOSE, "Adding resolution %d*%d to the supported ones", tWidth, tHeight);
		                                mSupportedVideoFormats.push_back(tFormat);
		                            }
								}
							}
						}
				    }
			    	delete [] tCaps;
				}
				tPin->Release();
			}
			tPinsEnum->Release();
		}

		tPropertyBag->Release();
		tMoniker->Release();
	}
	tClassEnumerator->Release();

    return mSupportedVideoFormats;
}

bool MediaSourceDShow::SupportsRecording()
{
	return true;
}

string MediaSourceDShow::GetCodecName()
{
    return "Raw";
}

string MediaSourceDShow::GetCodecLongName()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
