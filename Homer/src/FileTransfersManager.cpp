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
 * Purpose: Implementation of file transfers management
 * Author:  Thomas Volkert
 * Since:   2012-07-09
 */

#include <MediaSourceMem.h>
#include <MediaSourceNet.h>
#include <FileTransfersManager.h>
#include <ProcessStatisticService.h>
#include <HBSocket.h>
#include <HBTime.h>
#include <Header_Ffmpeg.h>

#include <stdio.h>
#include <sys/stat.h>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Base;
using namespace Homer::Monitor;

FileTransfersManager sFileTransfersManager;
uint32_t sLocalSourceId = 0;
uint32_t sLocalSessionId = 0;

///////////////////////////////////////////////////////////////////////////////

#define FTM_VERSION                         0

#define FTM_PDU_TRANSFER_BEGIN              0x01
#define FTM_PDU_TRANSFER_SUCCESS            0x02
#define FTM_PDU_TRANSFER_CANCEL             0x03
#define FTM_PDU_TRANSFER_PAUSE              0x04
#define FTM_PDU_TRANSFER_CONTINUE           0x05
#define FTM_PDU_TRANSFER_DATA               0x10
#define FTM_PDU_TRANSFER_SEEK               0x11

static string getNameFromPduType(int pType)
{
    switch(pType)
    {
        case FTM_PDU_TRANSFER_BEGIN:
            return "Transfer begin";
        case FTM_PDU_TRANSFER_SUCCESS:
            return "Transfer successfully finished";
        case FTM_PDU_TRANSFER_CANCEL:
            return "Transfer canceled";
        case FTM_PDU_TRANSFER_PAUSE:
            return "Transfer paused";
        case FTM_PDU_TRANSFER_CONTINUE:
            return "Transfer continued";
        case FTM_PDU_TRANSFER_DATA:
            return "Transfer data";
        case FTM_PDU_TRANSFER_SEEK:
            return "Transfer seek";
        default:
            "Unsupported PDU type";
    }
    return "";
}

union HomerFtmHeader{
    struct {

        uint32_t Session:16;            /* session ID */
        uint32_t PduType:7;             /* PDU type */
        uint32_t Request:1;             /* request flag */
        uint32_t mbz:5;                 /* must be zero */
        uint32_t Version:3;             /* protocol version */

        uint32_t Source;                /* source ID */

        uint32_t SequenceNumber;        /* time stamp */
    } __attribute__((__packed__));
    uint32_t Data[3];
};

union HomerFtmBeginHeader{
    struct{
        uint32_t     DataAcks:1;       /* request for data acknowledgments */
        uint32_t     mbz:31;           /* must be zero */
        uint32_t     Port;             /* sender's management port number */
        uint64_t     Size;             /* file size */
        char         Name[256];        /* file name */
    } __attribute__((__packed__));
    uint32_t Data[68];
};

union HomerFtmDataHeader{
    struct{
        uint64_t     Start;             /* fragment's start offset */
        uint64_t     End;               /* fragment's end offset */
    } __attribute__((__packed__));
    uint32_t Data[4];
};

union HomerFtmSeekHeader{
    struct{
        uint64_t     Position;          /* position in transfer stream */
    } __attribute__((__packed__));
    uint32_t Data[2];
};

///////////////////////////////////////////////////////////////////////////////

FileTransfersManagerObservable::FileTransfersManagerObservable()
{
    mFileTransfersManagerObserver = NULL;
}

FileTransfersManagerObservable::~FileTransfersManagerObservable()
{

}

void FileTransfersManagerObservable::notifyObserversTransferBegin(uint64_t pId, string pPeerName, string pFileName, uint64_t pFileSize)
{
    if (mFileTransfersManagerObserver != NULL)
        mFileTransfersManagerObserver->handleFileTransfersManagerEventTransferBegin(pId, pPeerName, pFileName, pFileSize);
}

void FileTransfersManagerObservable::notifyObserversTransferBeginRequest(uint64_t pId, string pPeerName, string pFileName, uint64_t pFileSize)
{
    if (mFileTransfersManagerObserver != NULL)
        mFileTransfersManagerObserver->handleFileTransfersManagerEventTransferBeginRequest(pId, pPeerName, pFileName, pFileSize);
}

void FileTransfersManagerObservable::notifyObserverTransferData(uint64_t pId, uint64_t pTransferredSize)
{
    if (mFileTransfersManagerObserver != NULL)
        mFileTransfersManagerObserver->handleFileTransfersManagerEventTransferData(pId, pTransferredSize);
}

void FileTransfersManagerObservable::AddObserver(FileTransfersManagerObserver *pObserver)
{
    mFileTransfersManagerObserver = pObserver;
}

void FileTransfersManagerObservable::DeleteObserver(FileTransfersManagerObserver *pObserver)
{
    mFileTransfersManagerObserver = NULL;
}

///////////////////////////////////////////////////////////////////////////////

class FileTransfer:
    public Thread
{
public:
    ~FileTransfer();

    /* ID management */
    uint64_t GetId();
    uint32_t GetSessionId();
    uint32_t GetSourceId();
    bool MatchesId(uint64_t pId);
    static uint64_t CreateId(uint32_t pSourceId, uint32_t pSessionId);

    /* file sender - thread */
    static FileTransfer* CreateFileSender(std::string pTargetName, Requirements *pTransportRequirements, std::string pFileName);
    static FileTransfer* CreateFileReceiver(IConnection *pSocketToPeer, HomerFtmHeader *pHeader, HomerFtmBeginHeader *pBeginHeader);

    bool IsSenderActive();

    /* control commands from GUI */
    void PauseFileTransfer();
    void ContinueFileTransfer();
    void CancelFileTransfer();
    // step 3 & 4 of 4 - way hand-shake for transfer start
    void ReceiverAcknowledgesTransferBegin(std::string pLocalFileName);

    /* interface to FTM: data (request/response) received */
    void FtmCallBack(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize);

private:
    FileTransfer();

    /* send thread */
    virtual void* Run(void* pArgs = NULL);
    void StopSendThread();

    /* data sending */
    bool SendPacket(char* pData, unsigned int pSize);

    /* sending requests */
    bool SendRequest(int pRequestType, char *pData, unsigned int pDataSize);
    // step 1 & 2 of 4 - way hand-shake for transfer start
    bool SendRequestTransferBegin();
    bool SendRequestTransferSuccess();
    bool SendRequestTransferCancel();
    bool SendRequestTransferPause();
    bool SendRequestTransferContinue();
    bool SendRequestTransferData(char *pData, unsigned int pDataSize, uint64_t pStart, uint64_t pEnd);
    bool SendRequestTransferSeek(uint64_t pPosition);

    /* acknowledge */
    bool AcknowledgeRequest(HomerFtmHeader *pHeader);

    /* received requests/responses */
    void ReceivedTransferBegin(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize);
        void ReceivedTransferBeginResponse(HomerFtmHeader *pHeader);
    void ReceivedTransferSuccess(HomerFtmHeader *pHeader);
        void ReceivedTransferSuccessResponse(HomerFtmHeader *pHeader);
    void ReceivedTransferCancel(HomerFtmHeader *pHeader);
        void ReceivedTransferCancelResponse(HomerFtmHeader *pHeader);
    void ReceivedTransferPause(HomerFtmHeader *pHeader);
        void ReceivedTransferPauseResponse(HomerFtmHeader *pHeader);
    void ReceivedTransferContinue(HomerFtmHeader *pHeader);
        void ReceivedTransferContinueResponse(HomerFtmHeader *pHeader);
    void ReceivedTransferData(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize);
        void ReceivedTransferDataResponse(HomerFtmHeader *pHeader);
    void ReceivedTransferSeek(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize);
        void ReceivedTransferSeekResponse(HomerFtmHeader *pHeader);

    bool                mRemoteClosedTransfer;
    bool                mUsesDataAcks;
    bool                mCanceled;
    Condition           mConditionRemoteWantsTransferBegin, mConditionRemoteAcksTransferData, mConditionRemoteAcksTransferEnd;
    unsigned int        mSessionId;
    unsigned int        mSourceId;
    uint64_t            mId;
    unsigned int        mSenderMessageCounter;
    unsigned int        mReceiverMessageCounter;
    char                *mPacketSendBuffer;
    bool                mSenderActive;
    FILE                *mLocalFile;
    Mutex               mFileReadMutex;
    std::string         mFileName;
    uint64_t            mFileSize;
    uint64_t            mFileTransferredSize;
    std::string         mPeerName;
    unsigned int        mPeerPort;
    Requirements        *mTransportRequirements;
    /* GAPI based transport */
    IConnection         *mSocketToPeer;
};

///////////////////////////////////////////////////////////////////////////////

static void CreateRequest(HomerFtmHeader *pHeader, unsigned int pSourceId, unsigned int pSessionId, unsigned int pMessageNumber, int pPduType)
{
    pHeader->Session = pSessionId;
    pHeader->PduType = pPduType;
    pHeader->Request = true;
    pHeader->mbz = 0;
    pHeader->Version = FTM_VERSION;
    pHeader->Source = pSourceId;
    pHeader->SequenceNumber = pMessageNumber;
}

///////////////////////////////////////////////////////////////////////////////
///// FILE Sender
///////////////////////////////////////////////////////////////////////////////

FileTransfer::FileTransfer()
{
    mCanceled = false;
    mLocalFile = NULL;
    mSourceId = 0;
    mSessionId = 0;
    mId = 0;
    mFileTransferredSize = 0;
    mSocketToPeer = NULL;
    mPacketSendBuffer = NULL;
    mSenderMessageCounter = 0;
    mReceiverMessageCounter = 0;
    mSenderActive = false;
    mUsesDataAcks = false;
    #ifdef FTM_ACK_DATA_PACKETS
        mUsesDataAcks = true; //TODO: check if GAPI socket provides lossless and activate ACKs here automatically
    #endif
}

FileTransfer::~FileTransfer()
{
    if (IsSenderActive())
    {
        LOG(LOG_VERBOSE, "Going to stop file sender thread");
        StopSendThread();
    }
    if (mPacketSendBuffer != NULL)
        free(mPacketSendBuffer);
    LOG(LOG_VERBOSE, "Destroyed");
}

bool FileTransfer::MatchesId(uint64_t pId)
{
    if (mId == pId)
        return true;
    else
        return false;
}

uint64_t FileTransfer::CreateId(uint32_t pSourceId, uint32_t pSessionId)
{
    return pSessionId + pSourceId * FTM_OFFSET_SOURCE_ID;
}

uint64_t FileTransfer::GetId()
{
    return CreateId(GetSourceId(), GetSessionId());
}

uint32_t FileTransfer::GetSessionId()
{
    return mSessionId;
}

uint32_t FileTransfer::GetSourceId()
{
    return mSourceId;
}

FileTransfer* FileTransfer::CreateFileSender(std::string pTargetName, Requirements *pTransportRequirements, std::string pFileName)
{
    FileTransfer* tTransfer = new FileTransfer();
    if (tTransfer != NULL)
    {
        struct stat tFileStatus;
        stat(pFileName.c_str(), &tFileStatus);
        tTransfer->mFileSize = tFileStatus.st_size;
        tTransfer->mFileName = pFileName;
        tTransfer->mPeerName = pTargetName;
        RequirementTargetPort *tReqPort = (RequirementTargetPort*)pTransportRequirements->get(RequirementTargetPort::type());
        if (tReqPort != NULL)
            tTransfer->mPeerPort = tReqPort->getPort();
        else
            tTransfer->mPeerPort = -1;
        tTransfer->mSourceId = sLocalSourceId;
        tTransfer->mSessionId = ++sLocalSessionId;
        tTransfer->mId = CreateId(tTransfer->mSourceId, tTransfer->mSessionId);
        tTransfer->mTransportRequirements = pTransportRequirements;
        tTransfer->mSocketToPeer = GAPI.connect(new Name(pTargetName), pTransportRequirements);
        if (tTransfer->mSocketToPeer == NULL)
            LOGEX(FileTransfer, LOG_ERROR, "Invalid GAPI socket");
        tTransfer->mPacketSendBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

        LOGEX(FileTransfer, LOG_VERBOSE, "Started file sender for file %s towards %s:%u", tTransfer->mFileName.c_str(), tTransfer->mPeerName.c_str(), tTransfer->mPeerPort);
        FTMAN.notifyObserversTransferBegin(tTransfer->mId, "localhost", tTransfer->mFileName, tTransfer->mFileSize);

        tTransfer->StartThread();
    }else
        LOGEX(FileTransfer, LOG_ERROR, "Invalid send transfer object");

    return tTransfer;
}

FileTransfer* FileTransfer::CreateFileReceiver(IConnection *pSocketToPeer, HomerFtmHeader *pRequestHeader, HomerFtmBeginHeader *pBeginHeader)
{
    uint64_t tFileSize = pBeginHeader->Size;
    string tFileName = string(pBeginHeader->Name);

    FileTransfer* tTransfer = new FileTransfer();
    if (tTransfer != NULL)
    {
        tTransfer->mFileSize = tFileSize;
        tTransfer->mFileName = tFileName;
        tTransfer->mPeerName = pSocketToPeer->getRemoteName()->toString();
        tTransfer->mPeerPort = -1;
        Requirements *tReqs = pSocketToPeer->getRequirements();
        if (tReqs != NULL)
        {
            RequirementTargetPort *tReqPort = (RequirementTargetPort*)tReqs->get(RequirementTargetPort::type());
            if (tReqPort != NULL)
                tTransfer->mPeerPort = tReqPort->getPort();
        }
        tTransfer->mSourceId = pRequestHeader->Source;
        tTransfer->mSessionId = pRequestHeader->Session;
        tTransfer->mId = CreateId(tTransfer->mSourceId, tTransfer->mSessionId);
        tTransfer->mTransportRequirements = pSocketToPeer->getRequirements();
        tTransfer->mSocketToPeer = pSocketToPeer;
        if (tTransfer->mSocketToPeer == NULL)
            LOGEX(FileTransfer, LOG_ERROR, "Invalid peer socket");
        tTransfer->mPacketSendBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

        LOGEX(FileTransfer, LOG_VERBOSE, "Started file receiver for file %s from %s:%u, source ID: %u, session ID: %u", tTransfer->mFileName.c_str(), tTransfer->mPeerName.c_str(), tTransfer->mPeerPort, tTransfer->mSourceId, tTransfer->mSessionId);
        FTMAN.notifyObserversTransferBegin(tTransfer->mId, tTransfer->mPeerName, tTransfer->mFileName, tTransfer->mFileSize);

    }else
        LOGEX(FileTransfer, LOG_ERROR, "Invalid receive transfer object");

    return tTransfer;
}

bool FileTransfer::IsSenderActive()
{
    return mSenderActive;
}

void FileTransfer::PauseFileTransfer()
{
    LOG(LOG_VERBOSE, "Pausing file transfer for %s", mFileName.c_str());
    if (!SendRequestTransferPause())
        LOG(LOG_ERROR, "SendRequestTransferPause() failed");
}

void FileTransfer::ContinueFileTransfer()
{
    LOG(LOG_VERBOSE, "Pausing file transfer for %s", mFileName.c_str());
    if (!SendRequestTransferContinue())
        LOG(LOG_ERROR, "SendRequestTransferContinue() failed");
}

void FileTransfer::CancelFileTransfer()
{
    LOG(LOG_VERBOSE, "Canceling file transfer for %s", mFileName.c_str());
    if (!SendRequestTransferCancel())
        LOG(LOG_ERROR, "SendRequestTransferCancel() failed");
}

void FileTransfer::ReceiverAcknowledgesTransferBegin(string pLocalFileName)
{
    LOG(LOG_VERBOSE, "Acknowledging file transfer for %s and trigger start for download to \"%s\"", mFileName.c_str(), pLocalFileName.c_str());
    mFileName = pLocalFileName;

    if (!SendRequestTransferBegin())
        LOG(LOG_ERROR, "SendRequestTransferBegin() failed");
    else
    {
        // open the local file
        mLocalFile = fopen(mFileName.c_str(), "wb");
    }
}

bool FileTransfer::SendPacket(char* pData, unsigned int pSize)
{
    bool tResult = false;

    int64_t tTime = Time::GetTimeStamp();
    if (mSocketToPeer != NULL)
    {
        mSocketToPeer->write(pData, (int)pSize);
        if (mSocketToPeer->isClosed())
        {
            LOG(LOG_ERROR, "Error when sending data through GAPI connection to %s:%u, will skip further transmissions", mPeerName.c_str(), mPeerPort);
        }else
            tResult = true;
    }else
        LOG(LOG_ERROR, "Invalid socket to peer");
    #ifdef FTM_DEBUG_TIMING
        int64_t tTime2 = Time::GetTimeStamp();
        LOG(LOG_VERBOSE, "       sending a packet of %u bytes took %ld us", pSize, tTime2 - tTime);
    #endif

    return tResult;
}

bool FileTransfer::SendRequest(int pRequestType, char *pData, unsigned int pDataSize)
{
    bool tResult = false;

    // prepare main header data
    HomerFtmHeader *tHeader = (HomerFtmHeader*) mPacketSendBuffer;
    CreateRequest(tHeader, mSourceId, mSessionId, ++mSenderMessageCounter, pRequestType);

    // derive entire request size
    unsigned int tDataSize = sizeof(HomerFtmHeader) + pDataSize;

    // is request size below maximum packet size?
    if (tDataSize <= MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE)
    {
        // prepare request payload data
        if (pDataSize > 0)
            memcpy(mPacketSendBuffer + sizeof(HomerFtmHeader), (const void*)pData, pDataSize);

        // send the request through the network
        LOG(LOG_VERBOSE, "+++++ Sending request of type \"%s\", source ID: %u, session ID: %u", getNameFromPduType(pRequestType).c_str(), tHeader->Source, tHeader->Session);
        tResult = SendPacket(mPacketSendBuffer, tDataSize);
    }else
        LOG(LOG_ERROR, "Packet is too big");

    return tResult;
}

bool FileTransfer::AcknowledgeRequest(HomerFtmHeader *pHeader)
{
    bool tResult = false;

    // prepare main header data
    pHeader->Request = 0;
    pHeader->SequenceNumber = ++mSenderMessageCounter;

    // derive entire request size
    unsigned int tDataSize = sizeof(HomerFtmHeader);

    // is request size below maximum packet size?
    if (tDataSize <= MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE)
    {
        // send the acknowledgment through the network
        LOG(LOG_VERBOSE, "+++++ Acknowledging request of type \"%s\"", getNameFromPduType(pHeader->PduType).c_str());
        tResult = SendPacket((char*)pHeader, tDataSize);
    }else
        LOG(LOG_ERROR, "Packet is too big");

    return tResult;
}

bool FileTransfer::SendRequestTransferBegin()
{
    bool tResult = false;

    HomerFtmBeginHeader tBeginHeader;

    // copy the file name into the header
    string tFileName = mFileName;

    // get file name without absolute path
    int tPos = (int)tFileName.rfind('\\');
    if (tPos == (int)string::npos)
        tPos = (int)tFileName.rfind('/');
    if (tPos != (int)string::npos)
    {
        tPos += 1;
        tFileName = tFileName.substr(tPos, tFileName.length() - tPos);
    }

    // copy file name into FTM begin header
    strcpy(tBeginHeader.Name, tFileName.c_str());

    // set file size in FTM begin header
    tBeginHeader.Size = mFileSize;

    // set own port number in FTM begin header
    tBeginHeader.Port = FTMAN.GetPort();

    // set own data ACK policy in FTM begin header
    tBeginHeader.DataAcks = mUsesDataAcks;

    tResult = SendRequest(FTM_PDU_TRANSFER_BEGIN, (char*)&tBeginHeader, sizeof(tBeginHeader));

    //TODO: signal to observer

    return tResult;
}

bool FileTransfer::SendRequestTransferSuccess()
{
    bool tResult = false;

    tResult = SendRequest(FTM_PDU_TRANSFER_SUCCESS, NULL, 0);

    return tResult;
}

bool FileTransfer::SendRequestTransferCancel()
{
    bool tResult = false;

    tResult = SendRequest(FTM_PDU_TRANSFER_CANCEL, NULL, 0);

    return tResult;
}

bool FileTransfer::SendRequestTransferPause()
{
    bool tResult = false;

    tResult = SendRequest(FTM_PDU_TRANSFER_PAUSE, NULL, 0);

    return tResult;
}

bool FileTransfer::SendRequestTransferContinue()
{
    bool tResult = false;

    tResult = SendRequest(FTM_PDU_TRANSFER_CONTINUE, NULL, 0);

    return tResult;
}

bool FileTransfer::SendRequestTransferData(char *pData, unsigned int pDataSize, uint64_t pStart, uint64_t pEnd)
{
    bool tResult = false;

    char tBuffer[FTM_DATA_PACKET_SIZE + sizeof(HomerFtmDataHeader)];
    HomerFtmDataHeader *tDataHeader = (HomerFtmDataHeader*)tBuffer;
    tDataHeader->Start = pStart;
    tDataHeader->End = pEnd;
    memcpy(tBuffer + sizeof(HomerFtmDataHeader) , pData, pDataSize);

    #ifdef FTM_DEBUG_DATA_PACKETS
        LOG(LOG_ERROR, "Sending of file \"%s\" the fragment from %lu to %lu towards %s:%u", mFileName.c_str(), tDataHeader->Start, tDataHeader->End, mPeerName.c_str(), mPeerPort);
    #endif

    tResult = SendRequest(FTM_PDU_TRANSFER_DATA, tBuffer, sizeof(HomerFtmDataHeader) + pDataSize);

    FTMAN.notifyObserverTransferData(GetId(), mFileTransferredSize);

    return tResult;
}

bool FileTransfer::SendRequestTransferSeek(uint64_t pPosition)
{
    bool tResult = false;

    HomerFtmSeekHeader tSeekHeader;

    // set file size in FTM begin header
    tSeekHeader.Position = pPosition;

    tResult = SendRequest(FTM_PDU_TRANSFER_SEEK, (char*)&tSeekHeader, sizeof(tSeekHeader));

    //TODO: signal to observer

    return tResult;
}

void FileTransfer::ReceivedTransferBegin(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize)
{
    if (pPayloadSize != sizeof(HomerFtmBeginHeader))
        LOG(LOG_WARN, "Size of payload differs from size of HomerFtmBeginHeader");

    HomerFtmBeginHeader *tBeginHeader = (HomerFtmBeginHeader*)pPayload;
    char tFileName[1024];
    if (strlen(tBeginHeader->Name) < 1024)
        strcpy(tFileName, tBeginHeader->Name);
    else
        strcpy(tFileName, "File name too long");

    LOG(LOG_VERBOSE, "Remote signaled transfer begin for file \"%s\" with size %ld", mFileName.c_str(), mFileSize);

    // send ACK
    AcknowledgeRequest(pHeader);

    // who are we?
    if (IsSenderActive())
    {// we are sender
        LOG(LOG_VERBOSE, "TRANSFER-BEGIN hand-shake finished, remote acknowledged transfer of file \"%s\" with size %ld", mFileName.c_str(), mFileSize);
        mConditionRemoteWantsTransferBegin.SignalAll();
    }else
    {// we are receiver
        mFileName = string (tFileName);
        mFileSize = tBeginHeader->Size;
        mUsesDataAcks = tBeginHeader->DataAcks;

        // ask the user about file destination on disc
        FTMAN.notifyObserversTransferBeginRequest(GetId(), mPeerName, mFileName, mFileSize);
    }
}

void FileTransfer::ReceivedTransferSuccess(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote signaled transfer successfully finished for file %s with size %ld", mFileName.c_str(), mFileSize);

    AcknowledgeRequest(pHeader);

    // who are we?
    if (IsSenderActive())
    {// we are sender
        LOG(LOG_VERBOSE, "TRANSFER-END hand-shake finished, remote acknowledged transfer of file \"%s\" with size %ld", mFileName.c_str(), mFileSize);
        mRemoteClosedTransfer = true;
        #ifdef FTM_DEBUG_DATA_PACKETS
            LOG(LOG_WARN, "Sending wake up to \"RemoteAcksTransferEnd\"");
        #endif
        mConditionRemoteAcksTransferEnd.SignalAll();
    }else
    {// we are receiver
        // does the file size match the desired one (was initially reported in TransferBegin)?
        if (mFileSize != mFileTransferredSize)
            LOG(LOG_WARN, "File size differs, received data: %lu bytes, expected data: %lu bytes", mFileTransferredSize, mFileSize);

        // tell the sender that we don't need more data and transfer can be closed
        if (mFileTransferredSize == mFileSize)
        {
            // close the file if we haven't done this already
            if (mLocalFile != NULL)
            {
                fclose(mLocalFile);
                mLocalFile = NULL;
            }

            SendRequestTransferSuccess();
        }
    }

    //TODO: notifyObserver
}

void FileTransfer::ReceivedTransferCancel(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote signaled transfer canceled for file %s with size %ld", mFileName.c_str(), mFileSize);

    AcknowledgeRequest(pHeader);

    mRemoteClosedTransfer = true;
    #ifdef FTM_DEBUG_DATA_PACKETS
        LOG(LOG_WARN, "Sending wake up to \"RemoteAcksTransferEnd\"");
    #endif
    mConditionRemoteAcksTransferEnd.SignalAll();

    //TODO: notifyObserver
}

void FileTransfer::ReceivedTransferPause(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote signaled transfer paused for file %s with size %ld", mFileName.c_str(), mFileSize);

    AcknowledgeRequest(pHeader);

    //TODO: notifyObserver
}

void FileTransfer::ReceivedTransferContinue(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote signaled transfer continued for file %s with size %ld", mFileName.c_str(), mFileSize);

    AcknowledgeRequest(pHeader);

    //TODO: notifyObserver
}

void FileTransfer::ReceivedTransferData(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize)
{
    HomerFtmDataHeader *tDataHeader = (HomerFtmDataHeader*)pPayload;
    uint64_t tFragmentSize = tDataHeader->End - tDataHeader->Start;
    #ifdef FTM_DEBUG_DATA_PACKETS
        LOG(LOG_VERBOSE, "Remote signaled %d bytes of transfer data for file %s with size %ld", (int)tFragmentSize, mFileName.c_str(), mFileSize);
    #endif

    if (tDataHeader->Start != mFileTransferredSize)
    {
        LOG(LOG_WARN, "Detected a gap in received transfer stream: %lu bytes successfully received, current fragment starts at position %lu", mFileTransferredSize, tDataHeader->Start);

        // ACK this fragment
        if (mUsesDataAcks)
            AcknowledgeRequest(pHeader);

        // seek within sender's stream to position after last successfully received byte
        SendRequestTransferSeek(mFileTransferredSize);

        return;
    }

    if (mLocalFile != NULL)
    {
        #ifdef FTM_DEBUG_DATA_PACKETS
            LOG(LOG_ERROR, "Storing in file \"%s\" the fragment from %lu to %lu", mFileName.c_str(), tDataHeader->Start, tDataHeader->End);
        #endif

        char *tFragment = pPayload + sizeof(HomerFtmDataHeader);

        // write fragment to local file
        size_t tResult = fwrite((void*)tFragment, 1, (size_t)tFragmentSize, mLocalFile);
        if (tResult != tFragmentSize)
            LOG(LOG_ERROR, "Error when writing fragment to file, wrote %d bytes instead of %lu bytes", (int)tResult, tFragmentSize);
        else
            mFileTransferredSize += tFragmentSize;
    }

    // only acknowledge data reception if the sender has requested this during hand-shake process
    if (mUsesDataAcks)
        AcknowledgeRequest(pHeader);

    FTMAN.notifyObserverTransferData(GetId(), mFileTransferredSize);
}

void FileTransfer::ReceivedTransferSeek(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize)
{
    if (pPayloadSize != sizeof(HomerFtmSeekHeader))
        LOG(LOG_WARN, "Size of payload differs from size of HomerFtmBeginHeader");

    HomerFtmSeekHeader *tSeekHeader = (HomerFtmSeekHeader*)pPayload;

    LOG(LOG_VERBOSE, "Remote signaled transfer seek to %lu for file \"%s\" with size %ld", tSeekHeader->Position, mFileName.c_str(), mFileSize);

    mFileReadMutex.lock();

    if (mLocalFile != NULL)
    {
        mFileTransferredSize = tSeekHeader->Position;
        int tRes = 0;
        if ((tRes = fseek(mLocalFile, mFileTransferredSize, SEEK_SET)) < 0)
        {
            LOG(LOG_ERROR, "File seeking failed because \"%s\"", strerror(tRes));
        }
    }else
        LOG(LOG_WARN, "Local file %s is closed", mFileName.c_str());

    mFileReadMutex.unlock();

    // send ACK
    AcknowledgeRequest(pHeader);

    #ifdef FTM_DEBUG_DATA_PACKETS
        LOG(LOG_WARN, "Sending wake up to \"RemoteAcksTransferEnd\" because of seek request");
    #endif
    mConditionRemoteAcksTransferEnd.SignalAll();

    //TODO: notifyObserver
}

void FileTransfer::ReceivedTransferBeginResponse(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote acknowledged begin of transfer data for file %s with size %ld", mFileName.c_str(), mFileSize);

    //TODO: timeout handling
}

void FileTransfer::ReceivedTransferSuccessResponse(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote acknowledged transfer successfully finished for file %s with size %ld", mFileName.c_str(), mFileSize);

    //TODO: timeout handling
}

void FileTransfer::ReceivedTransferCancelResponse(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote acknowledged transfer canceled for file %s with size %ld", mFileName.c_str(), mFileSize);

    //TODO: timeout handling
}

void FileTransfer::ReceivedTransferPauseResponse(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote acknowledged transfer paused for file %s with size %ld", mFileName.c_str(), mFileSize);

    //TODO: timeout handling
}

void FileTransfer::ReceivedTransferContinueResponse(HomerFtmHeader *pHeader)
{
    LOG(LOG_VERBOSE, "Remote acknowledged transfer continued for file %s with size %ld", mFileName.c_str(), mFileSize);

    //TODO: timeout handling
}

void FileTransfer::ReceivedTransferDataResponse(HomerFtmHeader *pHeader)
{
    #ifdef FTM_DEBUG_DATA_PACKETS
        LOG(LOG_VERBOSE, "Remote acknowledged transfer data for file %s with size %ld", mFileName.c_str(), mFileSize);
    #endif

    mConditionRemoteAcksTransferData.SignalAll();

    //TODO: timeout handling
}

void FileTransfer::ReceivedTransferSeekResponse(HomerFtmHeader *pHeader)
{
    #ifdef FTM_DEBUG_DATA_PACKETS
        LOG(LOG_VERBOSE, "Remote acknowledged transfer seek for file %s with size %ld", mFileName.c_str(), mFileSize);
    #endif

    //TODO: timeout handling
}

void FileTransfer::FtmCallBack(HomerFtmHeader *pHeader, char *pPayload, unsigned int pPayloadSize)
{
    if (pHeader->Request == true)
        LOG(LOG_VERBOSE, "===== Received file transfer request event of type \"%s\" =====", getNameFromPduType(pHeader->PduType).c_str());
    else
        LOG(LOG_VERBOSE, "===== Received file transfer response event of type \"%s\" =====", getNameFromPduType(pHeader->PduType).c_str());

    if (mReceiverMessageCounter != 0)
    {
        int tDiff = pHeader->SequenceNumber - mReceiverMessageCounter;
        if (tDiff != 1)
            LOG(LOG_ERROR, "Unexpected sequence number in received message, sequence number differs by %d from last seen sequence number", tDiff);
    }

    switch(pHeader->PduType)
    {
        case FTM_PDU_TRANSFER_BEGIN:
            if (pHeader->Request == true)
                ReceivedTransferBegin(pHeader, pPayload, pPayloadSize);
            else
                ReceivedTransferBeginResponse(pHeader);
            break;
        case FTM_PDU_TRANSFER_SUCCESS:
            if (pHeader->Request == true)
                ReceivedTransferSuccess(pHeader);
            else
                ReceivedTransferSuccessResponse(pHeader);
            break;
        case FTM_PDU_TRANSFER_CANCEL:
            if (pHeader->Request == true)
                ReceivedTransferCancel(pHeader);
            else
                ReceivedTransferCancelResponse(pHeader);
            break;
        case FTM_PDU_TRANSFER_PAUSE:
            if (pHeader->Request == true)
                ReceivedTransferPause(pHeader);
            else
                ReceivedTransferPauseResponse(pHeader);
            break;
        case FTM_PDU_TRANSFER_CONTINUE:
            if (pHeader->Request == true)
                ReceivedTransferContinue(pHeader);
            else
                ReceivedTransferContinueResponse(pHeader);
            break;
        case FTM_PDU_TRANSFER_DATA:
            if (pHeader->Request == true)
                ReceivedTransferData(pHeader, pPayload, pPayloadSize);
            else
                ReceivedTransferDataResponse(pHeader);
            break;
        case FTM_PDU_TRANSFER_SEEK:
            if (pHeader->Request == true)
                ReceivedTransferSeek(pHeader, pPayload, pPayloadSize);
            else
                ReceivedTransferSeekResponse(pHeader);
            break;
        default:
            LOG(LOG_WARN, "Unsupported PDU type");
            break;
    }
}

void FileTransfer::StopSendThread()
{
    mCanceled = true;
    mSocketToPeer->cancel();
    mConditionRemoteWantsTransferBegin.SignalAll();
    StopThread(2000);
}

void* FileTransfer::Run(void* pArgs)
{
    LOG(LOG_VERBOSE, "Started thread for sending file %s", mFileName.c_str());
    mSenderActive = true;

    if (mTransportRequirements->contains(RequirementTargetPort::type()))
    {
        RequirementTargetPort *tReqPort = (RequirementTargetPort*)mTransportRequirements->get(RequirementTargetPort::type());
        mPeerPort = tReqPort->getPort();
    }else
    {
        RequirementTargetPort *tReqPort = new RequirementTargetPort(FTM_DEFAULT_PORT);
        mTransportRequirements->add(tReqPort);
    }

    if (SendRequestTransferBegin())
    {
        // wait until hand-shake is completed
        mConditionRemoteWantsTransferBegin.Reset();
        mConditionRemoteWantsTransferBegin.Wait();

        if (!mCanceled)
        {
            LOG(LOG_VERBOSE, ">>> Going to start transmission of file \"%s\" to %s:%u", mFileName.c_str(), mPeerName.c_str(), mPeerPort);
            if ((mLocalFile = fopen(mFileName.c_str(), "rb")) != NULL)
            {
                char tBuffer[FTM_DATA_PACKET_SIZE];
                size_t tResult = 0;
                bool tEof = false;
                mRemoteClosedTransfer = false;
                do
                {
                    do
                    {
                        // ###################################################
                        // read fragment from local file
                        // ###################################################
                        int64_t tStart = INT_MAX;
                        int64_t tEnd = 0;
                        mFileReadMutex.lock();
                        if (!feof(mLocalFile))
                        {
                            tStart = ftell(mLocalFile);
                            tResult = fread((void*)tBuffer, 1, FTM_DATA_PACKET_SIZE, mLocalFile);
                            if (tResult != FTM_DATA_PACKET_SIZE)
                            {
                                if (!feof(mLocalFile))
                                    LOG(LOG_ERROR, "Error when reading in local file \"%s\"", mFileName.c_str());
                                else
                                    tEof = true;
                            }
                            tEnd = ftell(mLocalFile);
                            mFileTransferredSize = tEnd;
                        }else
                            LOG(LOG_VERBOSE, "EOF reached");
                        mFileReadMutex.unlock();

                        // ###################################################
                        // transmit file fragment (and retransmit if necessary)
                        // ###################################################
                        bool tRetransmission;
                        if (tStart < tEnd)
                        {
                            do{
                                tRetransmission = false;
                                SendRequestTransferData(tBuffer, tResult, tStart, tEnd);
                                if(mUsesDataAcks)
                                {
                                    // wait until hand-shake is completed
                                    mConditionRemoteAcksTransferData.Reset();
                                    if (!mConditionRemoteAcksTransferData.Wait(NULL, FTM_DATA_ACK_TIME))
                                    {
                                        LOG(LOG_WARN, "Timeout occurred while waiting for ACK of data transfer, retransmitting the packet");
                                        tRetransmission = true;
                                    }
                                }
                            }while(tRetransmission);
                        }
                    }while(!tEof);

                    LOG(LOG_VERBOSE, ">>> Ready to finish transmission of file \"%s\" to %s:%u, waiting for peer", mFileName.c_str(), mPeerName.c_str(), mPeerPort);
                    SendRequestTransferSuccess();

                    // wait until hand-shake is completed
                    mConditionRemoteAcksTransferEnd.Reset();
                    if (!mConditionRemoteAcksTransferEnd.Wait(NULL, FTM_KEEP_ALIVE_TIME * 2))
                    {
                        LOG(LOG_WARN, "Timeout occurred while waiting for ACK of data transfer end");
                        mRemoteClosedTransfer = true;
                    }else
                    {
                        #ifdef FTM_DEBUG_DATA_PACKETS
                            LOG(LOG_WARN, "Returned from conditional waiting for a remote ACK for transfer end, remote closed transfer: %d", mRemoteClosedTransfer);
                        #endif
                    }
                }while(!mRemoteClosedTransfer);

                fclose(mLocalFile);
                mLocalFile = NULL;
            }else
                LOG(LOG_ERROR, "Unable to open file %s", mFileName.c_str());

            LOG(LOG_VERBOSE, ">>> Going to finish successfully the transmission of file \"%s\" to %s:%u", mFileName.c_str(), mPeerName.c_str(), mPeerPort);

        }else
            LOG(LOG_VERBOSE, "Transfer was canceled");
    }

    mSenderActive = false;
    LOG(LOG_VERBOSE, "Finished thread for sending file %s", mFileName.c_str());

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///// FILE Transfer Manager
///////////////////////////////////////////////////////////////////////////////

FileTransfersManager::FileTransfersManager()
{
    mFileListenerNeeded = false;
    mFileListenerRunning = false;
    mReceiveErrors = 0;
    mLocalPort = 0;
    sLocalSourceId = av_get_random_seed();
    mSourceId = sLocalSourceId;
    mPacketReceiveBuffer = NULL;
}

FileTransfersManager::~FileTransfersManager()
{
    if (mPacketReceiveBuffer != NULL)
    {
        StopListener();
        free(mPacketReceiveBuffer);
    }

    if (mGAPIBinding != NULL)
    {
        LOG(LOG_VERBOSE, "..destroying GAPI bind object");
        delete mGAPIBinding; //HINT: this stops all listeners automatically
    }

    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

FileTransfersManager& FileTransfersManager::getInstance()
{
    return (sFileTransfersManager);
}

void FileTransfersManager::Init(string pLocalName, Requirements *pTransportRequirements)
{
    LOG(LOG_VERBOSE, "Initializing file transfer manager..");
    mLocalName = pLocalName;
    mTransportRequirements = pTransportRequirements;

    RequirementTargetPort *tReqPort = NULL;
    if (pTransportRequirements->contains(RequirementTargetPort::type()))
    {
        tReqPort = (RequirementTargetPort*)pTransportRequirements->get(RequirementTargetPort::type());
    }else
    {
        tReqPort = new RequirementTargetPort(FTM_DEFAULT_PORT);
        pTransportRequirements->add(tReqPort);
    }

    Name tName(pLocalName);
    mGAPIBinding = GAPI.bind(&tName, pTransportRequirements);
    if (mGAPIBinding == NULL)
        LOG(LOG_ERROR, "Invalid GAPI setup interface, name is %s, requirements are %s", pLocalName.c_str(), pTransportRequirements->getDescription().c_str());
    mReceiverSocket = mGAPIBinding->readConnection();
    if (mReceiverSocket == NULL)
        LOG(LOG_ERROR, "Invalid GAPI association, name is %s, requirements are %s", pLocalName.c_str(), pTransportRequirements->getDescription().c_str());

    mLocalPort = tReqPort->getPort();
    mPacketReceiveBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
    StartListener();
}

unsigned int FileTransfersManager::GetPort()
{
    return mLocalPort;
}

uint32_t FileTransfersManager::GetSourceId()
{
    return mSourceId;
}

uint32_t FileTransfersManager::Id2SourceId(uint64_t pId)
{
    uint64_t tResult = (pId & FTM_MASK_SOURCE_ID) >> 32;

    return (uint32_t)tResult;
}

bool FileTransfersManager::IsFromLocalhost(uint64_t pId)
{
    LOG(LOG_VERBOSE, "Check if local host, local source ID: %u, checking ID: %u : %u", mSourceId, Id2SourceId(pId), (uint32_t)(pId & FTM_MASK_SESSION_ID));
    return (Id2SourceId(pId) == mSourceId);
}

uint64_t FileTransfersManager::SendFile(std::string pTargetName, Requirements *pTransportRequirements, std::string pFileName)
{
    uint64_t tResult = 0;

    FileTransfer *tFileTransfer = FileTransfer::CreateFileSender(pTargetName, pTransportRequirements, pFileName);
    tResult = tFileTransfer->GetId();

    RegisterTransfer(tFileTransfer);

    return tResult;
}

void FileTransfersManager::AcknowledgeTransfer(uint64_t pId, std::string pLocalFileName)
{
    LOG(LOG_VERBOSE, "Acknowledging transfer %lu, local file name defined as \"%s\"", pId, pLocalFileName.c_str());
    FileTransfer *tTransfer = SearchTransfer(pId);
    if (tTransfer != NULL)
        tTransfer->ReceiverAcknowledgesTransferBegin(pLocalFileName);
}

void FileTransfersManager::PauseTransfer(uint64_t pId)
{
    LOG(LOG_VERBOSE, "Pausing transfer %lu", pId);
    FileTransfer *tTransfer = SearchTransfer(pId);
    if (tTransfer != NULL)
        tTransfer->PauseFileTransfer();
}

void FileTransfersManager::ContinueTransfer(uint64_t pId)
{
    LOG(LOG_VERBOSE, "Continuing transfer %lu", pId);
    FileTransfer *tTransfer = SearchTransfer(pId);
    if (tTransfer != NULL)
        tTransfer->ContinueFileTransfer();
}

void FileTransfersManager::CancelTransfer(uint64_t pId)
{
    LOG(LOG_VERBOSE, "Canceling transfer %lu", pId);
    FileTransfer *tTransfer = SearchTransfer(pId);
    if (tTransfer != NULL)
        tTransfer->CancelFileTransfer();
}

FileTransfer* FileTransfersManager::SearchTransfer(uint64_t pId)
{
    FileTransfer *tResult = NULL;
    FileTransfers::iterator tIt;

    mFileTransfersMutex.lock();

    for (tIt = mFileTransfers.begin(); tIt != mFileTransfers.end(); tIt++)
    {
        if ((*tIt)->MatchesId(pId))
        {
            tResult = *tIt;
            break;
        }
    }

    mFileTransfersMutex.unlock();

    return tResult;
}

void FileTransfersManager::RegisterTransfer(FileTransfer* pInstance)
{
    mFileTransfersMutex.lock();

    mFileTransfers.push_back(pInstance);

    mFileTransfersMutex.unlock();
}


///////////////////////////////////////////////////////////////////////////////
///// Listener
///////////////////////////////////////////////////////////////////////////////

void FileTransfersManager::StartListener()
{
    StartThread();
}

void FileTransfersManager::StopListener()
{
    mFileListenerNeeded = false;
    mReceiverSocket->cancel();
    StopThread(2000);
}

bool FileTransfersManager::ReceivePacket(std::string &pSourceHost, unsigned int &pSourcePort, char* pData, int &pSize)
{
    bool tResult = false;

    if (mReceiverSocket != NULL)
    {
        mReceiverSocket->read(pData, pSize);
        if (!mReceiverSocket->isClosed())
        {// success
            tResult = true;
            string tRemoteName = mReceiverSocket->getRemoteName()->toString();
            int tPos = tRemoteName.find('<');
            if (tPos != (int)string::npos)
                tRemoteName = tRemoteName.substr(0, tPos);
            pSourceHost = tRemoteName;
            pSourcePort = 1;
        }
    }else
        LOG(LOG_ERROR, "Invalid GAPI association, name is %s, requirements are %s", mLocalName.c_str(), mTransportRequirements->getDescription().c_str());
    return tResult;
}

void* FileTransfersManager::Run(void* pArgs)
{
    string tSourceHost = "";
    unsigned int tSourcePort = 0;
    int tDataSize;

    SVC_PROCESS_STATISTIC.AssignThreadName("FileTransfer-Listener");

    if (mReceiverSocket == NULL)
        return NULL;

    mFileListenerNeeded = true;

    while(mFileListenerNeeded)
    {
        //##############################
        // receive packet from network
        // #############################
        tDataSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE;
        tSourceHost = "";
        if (!ReceivePacket(tSourceHost, tSourcePort, mPacketReceiveBuffer, tDataSize))
        {
            if (mReceiveErrors == MEDIA_SOURCE_NET_MAX_RECEIVE_ERRORS)
            {
                LOG(LOG_ERROR, "Maximum number of continuous receive errors(%d) is exceeded, will stop network listener", MEDIA_SOURCE_NET_MAX_RECEIVE_ERRORS);
                mFileListenerRunning = false;
                break;
            }else
                mReceiveErrors++;
        }else
            mReceiveErrors = 0;
        if (tDataSize > 0)
        {
            //############################################
            // parse main header and find transfer object
            // ###########################################
            HomerFtmHeader *tHeader = (HomerFtmHeader*)mPacketReceiveBuffer;

            if (tHeader->Request == true)
                LOG(LOG_VERBOSE, "============= Received request event of type \"%s\" (source: %u, session: %u) =============", getNameFromPduType(tHeader->PduType).c_str(), tHeader->Source, tHeader->Session);
            else
                LOG(LOG_VERBOSE, "============= Received response event of type \"%s\" (source: %u, session: %u) =============", getNameFromPduType(tHeader->PduType).c_str(), tHeader->Source, tHeader->Session);

            char *tPayload = mPacketReceiveBuffer + sizeof(HomerFtmHeader);
            int tPayloadSize = tDataSize - sizeof(HomerFtmHeader);
            uint64_t tId = FileTransfer::CreateId(tHeader->Source, tHeader->Session);
            FileTransfer *tFileTransfer = SearchTransfer(tId);
            if (tFileTransfer == NULL)
            {// new ID found: incoming new file transfer request found, creating new receiver object
                if (tHeader->PduType == FTM_PDU_TRANSFER_BEGIN)
                {
                    HomerFtmBeginHeader *tBeginHeader = (HomerFtmBeginHeader*)(mPacketReceiveBuffer + sizeof(HomerFtmHeader));

                    Requirements *tRequs = new Requirements();
                    RequirementTransmitChunks *tReqChunks = new RequirementTransmitChunks();
                    RequirementTransmitLossless *tReqLossLess = new RequirementTransmitLossless();
                    RequirementTargetPort *tReqPort = new RequirementTargetPort((int)tBeginHeader->Port);

                    tRequs->add(tReqChunks);
                    tRequs->add(tReqLossLess);

                    // add the peer port to requirements
                    tRequs->add(tReqPort);

                    // create response socket
                    IConnection *tSocket = GAPI.connect(new Name(tSourceHost), tRequs);

                    // create transfer object
                    tFileTransfer = FileTransfer::CreateFileReceiver(tSocket, tHeader, tBeginHeader);

                    // register new instance in internal database
                    RegisterTransfer(tFileTransfer);
                }else
                    LOG(LOG_WARN, "Unexpected PDU of type %s from %s:%u received, will ignore this", getNameFromPduType(tHeader->PduType).c_str(), tSourceHost.c_str(), tSourcePort);
            }

            // forward received data to correct receiver object
            tFileTransfer->FtmCallBack(tHeader, tPayload, tPayloadSize);
        }
    }

    mFileListenerRunning = false;
    LOG(LOG_VERBOSE, "FileTransfer-Listener for port %d stopped", mLocalPort);

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
