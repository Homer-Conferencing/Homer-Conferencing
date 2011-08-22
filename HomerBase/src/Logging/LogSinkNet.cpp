/*
 * Name:    LogSinkNet.cpp
 * Purpose: Implementation of log sink for file output
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#include <LogSinkNet.h>
#include <Logger.h>

#include <string>
#include <stdio.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////////////////////////////////////////////

LogSinkNet::LogSinkNet(string pTargetHost, unsigned short pTargetPort)
{
    mBrokenPipe = false;
    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;
    mMediaId = "NET: " + mTargetHost + "<" + toString(mTargetPort) + ">";
    if ((mTargetHost != "") && (mTargetPort != 0))
        mDataSocket = new Socket(IS_IPV6_ADDRESS(mTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_UDP);
    printf("Logging debug output to net %s:%u\n", mTargetHost.c_str(), mTargetPort);
}

LogSinkNet::~LogSinkNet()
{
}

///////////////////////////////////////////////////////////////////////////////

void LogSinkNet::ProcessMessage(int pLevel, string pTime, string pSource, int pLine, string pMessage)
{
    if (mDataSocket == NULL)
        return;

    if ((mTargetHost == "") || (mTargetPort == 0))
    {
        LOG(LOG_ERROR, "Remote network address invalid");
        return;
    }
    if (mBrokenPipe)
    {
        LOG(LOG_VERBOSE, "Skipped fragment transmission because of broken pipe");
        return;
    }

    string tData;
    tData += "LEVEL=" + toString(pLevel) + "\n";
    tData += "TIME=" + pTime + "\n";
    tData += "SOURCE=" + pSource + "\n";
    tData += "LINE=" + toString(pLine) + "\n";
    tData += "MESSAGE=" + pMessage + "\n";

    if (!mDataSocket->Send(mTargetHost, mTargetPort, (void*)tData.c_str(), (ssize_t)tData.size()))
    {
        LOG(LOG_ERROR, "Error when sending data through UDP socket to %s:%u, will skip further transmissions", mTargetHost.c_str(), mTargetPort);
        mBrokenPipe = true;
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
