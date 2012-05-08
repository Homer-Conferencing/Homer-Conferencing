/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Wave output base class
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 */

#include <WaveOut.h>
#include <Logger.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

WaveOut::WaveOut(string pName):
    PacketStatistic(pName)
{
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);
    mPlaybackStopped = true;
    mWaveOutOpened = false;
    mDesiredDevice = "";
    mCurrentDevice = "";
    mVolume = 100;
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

void WaveOut::Stop()
{
	LOG(LOG_VERBOSE, "Mark stream as stopped");
    mPlaybackStopped = true;
}

bool WaveOut::Play()
{
	LOG(LOG_VERBOSE, "Mark stream as started");
    mPlaybackStopped = false;

    return true;
}

int WaveOut::GetVolume()
{
    return mVolume;
}

void WaveOut::SetVolume(int pValue)
{
    if (pValue < 0)
        pValue = 0;
    if(pValue > 200)
        pValue = 200;
    mVolume = pValue;
}

}} //namespaces
