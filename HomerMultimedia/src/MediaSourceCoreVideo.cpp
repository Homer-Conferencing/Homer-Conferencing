/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: CoreVideo capture implementation for OSX
 * Since:   2011-11-16
 */

#include <MediaSourceCoreVideo.h>
#include <ProcessStatisticService.h>
#include <Logger.h>

#include <string.h>
#include <stdlib.h>

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <QuickTime/QuickTime.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

MediaSourceCoreVideo::MediaSourceCoreVideo(string pDesiredDevice):
    MediaSource("CoreVideo: local capture")
{
    mSourceType = SOURCE_DEVICE;
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new CoreVideo device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

MediaSourceCoreVideo::~MediaSourceCoreVideo()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();

    LOG(LOG_VERBOSE, "Destroyed");
}

void MediaSourceCoreVideo::getVideoDevices(VideoDevices &pVList)
{
    static bool             tFirstCall = true;
    VideoDeviceDescriptor     tDevice;
    OSErr                      tRes = noErr;
    //SGDeviceList             tDeviceList = 0;

    #ifdef MSCV_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

#if 0
    ComponentDescription theDesc;
    Component sgCompID;
    ComponentResult result;
    theDesc.componentType = SeqGrabComponentType;
    theDesc.componentSubType = 0L;
    theDesc.componentManufacturer = 'appl';
    theDesc.componentFlags = 0L;
    theDesc.componentFlagsMask = 0L;
    sgCompID = FindNextComponent(NULL, &theDesc);
    seqGrabber = OpenComponent(sgCompID);
    result = SGInitialize(seqGrabber);
    result = SGNewChannel(seqGrabber, VideoMediaType, &videoChannel);
    SGDeviceList theDevices;
    SGGetChannelDeviceList(videoChannel,
            sgDeviceListDontCheckAvailability | sgDeviceListIncludeInputs,
            &theDevices);

    if (theDevices) {
        int theDeviceIndex;
        for (theDeviceIndex = 0; theDeviceIndex != (*theDevices)->count;
                ++theDeviceIndex) {
            SGDeviceName theDeviceEntry = (*theDevices)->entry[theDeviceIndex];
            cout << i << ".1. " << theDeviceEntry.name << endl;
            // name of device is a pstring in theDeviceEntry.name

            SGDeviceInputList theInputs = theDeviceEntry.inputs;
            if (theInputs != NULL) {
                int theInputIndex;
                for (theInputIndex = 0; theInputIndex != (*theInputs)->count;
                        ++theInputIndex) {
                    SGDeviceInputName theInput =
                            (*theInputs)->entry[theInputIndex];
                    cout << i << ".2. " << theInput.name << endl;
                    // name of input is a pstring in theInput.name
                }
            }
        }
    }
#endif



    tFirstCall = false;
}

bool MediaSourceCoreVideo::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    int                 tResult;
//    AVFormatParameters  tFormatParams;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(VFW)");

    if (mMediaSourceOpened)
        return false;

//    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
//        mDesiredDevice = "";

    //TODO: implement

    mMediaType = MEDIA_VIDEO;
    MarkOpenGrabDeviceSuccessful();

    return true;
}

bool MediaSourceCoreVideo::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceCoreVideo::CloseGrabDevice()
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

        //TODO: implement

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceCoreVideo::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    int tResult;

    // lock grabbing
    mGrabMutex.lock();

    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("grab buffer is NULL");

        return -1;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        MarkGrabChunkFailed("video source is closed");

        return GRAB_RES_INVALID;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        MarkGrabChunkFailed("video source is paused");

        return GRAB_RES_INVALID;
    }

    //TODO: implement

    //pChunkSize = mSampleBufferSize;

    // re-encode the frame and write it to file
//    if (mRecording)
//        RecordSamples((int16_t *)pChunkBuffer, pChunkSize);

    // unlock grabbing
    mGrabMutex.unlock();

    // log statistics about raw PCM audio data stream
//    AnnouncePacket(pChunkSize);

    mFrameNumber++;

    // acknowledge success
    MarkGrabChunkSuccessful(mFrameNumber);

    return mFrameNumber;
}

bool MediaSourceCoreVideo::SupportsRecording()
{
    return true;
}

string MediaSourceCoreVideo::GetSourceCodecStr()
{
    return "Raw";
}

string MediaSourceCoreVideo::GetSourceCodecDescription()
{
    return "Raw";
}

}} //namespaces
