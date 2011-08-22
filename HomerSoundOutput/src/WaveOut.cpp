/*
 * Name:    WaveOut.cpp
 * Purpose: Wave output base class
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 * Version: $Id$
 */

#include <WaveOut.h>
#include <Logger.h>

namespace Homer { namespace SoundOutput {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Multimedia;

WaveOut::WaveOut(string pName):
    PacketStatistic(pName)
{
    ClassifyStream(DATA_TYPE_AUDIO, PACKET_TYPE_RAW);
    mPlaybackStopped = false;
    mWaveOutOpened = false;
    mDesiredDevice = "";
    mCurrentDevice = "";
    mChunkNumber = 0;
}

WaveOut::~WaveOut()
{
}

///////////////////////////////////////////////////////////////////////////////

bool WaveOut::SelectDevice(string pDeviceName)
{
    AudioDevicesList::iterator tAIt;
    AudioDevicesList tAList;
    string tOldDesiredDevice = mDesiredDevice;
    bool tNewSelection = false;
    bool tAutoSelect = false;

    LOG(LOG_INFO, "Selecting new device..");

    if ((pDeviceName == "auto") || (pDeviceName == ""))
        tAutoSelect = true;

    getAudioDevices(tAList);
    for (tAIt = tAList.begin(); tAIt != tAList.end(); tAIt++)
    {
        if ((pDeviceName == tAIt->Name) || (tAutoSelect))
        {
            mDesiredDevice = tAIt->Card;
            if (mDesiredDevice != mCurrentDevice)
                LOG(LOG_INFO, "..audio device selected: %s", mDesiredDevice.c_str());
            else
                LOG(LOG_INFO, "..audio device re-selected: %s", mDesiredDevice.c_str());
            tNewSelection = true;
        }
    }

    if ((!tNewSelection) && (((pDeviceName == "auto") || (pDeviceName == "automatic") || pDeviceName == "")))
    {
        LOG(LOG_INFO, "..device selected: auto detect (card: auto)");
        mDesiredDevice = "";
    }

    LOG(LOG_VERBOSE, "WaveOut should be reseted: %d", tNewSelection);

    return tNewSelection;
}

void WaveOut::StopPlayback()
{
    mPlaybackStopped = true;
}

}} //namespaces
