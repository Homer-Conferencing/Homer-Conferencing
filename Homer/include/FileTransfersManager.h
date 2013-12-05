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
 * Purpose: File transfers management
 * Since:   2012-07-09
 */

#ifndef _FILE_TRANSFER_MANAGER_
#define _FILE_TRANSFER_MANAGER_

#include <NAPI.h>
#include <Requirements.h>
#include <string.h>
#include <HBMutex.h>
#include <HBThread.h>

namespace Homer { namespace Gui {

using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

// de/activate acknowledgments of file data packets
//#define FTM_ACK_DATA_PACKETS

//#define FTM_DEBUG_TIMING

#define FTM_DEBUG_DATA_PACKETS

///////////////////////////////////////////////////////////////////////////////

#define FTM_DEFAULT_PORT                6000
#define FTM_DATA_PACKET_SIZE            1200

// timeouts
#define FTM_DATA_ACK_TIME               3000 // ms
#define FTM_KEEP_ALIVE_TIME             5000 // ms

#define FTM_OFFSET_SOURCE_ID            ((uint64_t)0x0000000100000000LLU)
#define FTM_MASK_SOURCE_ID              ((uint64_t)0xFFFFFFFF00000000LLU)
#define FTM_MASK_SESSION_ID             ((uint64_t)0x00000000FFFFFFFFLLU)

///////////////////////////////////////////////////////////////////////////////

#define FTMAN FileTransfersManager::getInstance()

///////////////////////////////////////////////////////////////////////////////

class FileTransfersManagerObserver
{
public:
    FileTransfersManagerObserver() { }

    virtual ~FileTransfersManagerObserver() { }

    /* handling function for FileTransfersManager events */
    virtual void handleFileTransfersManagerEventTransferBeginRequest(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize) = 0;
    virtual void handleFileTransfersManagerEventTransferBegin(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize) = 0;
    virtual void handleFileTransfersManagerEventTransferData(uint64_t pId, uint64_t pTransferredSize) = 0;
};

//////////////////////////////////////////////////////////////////////////////

class FileTransfersManagerObservable
{
public:
    FileTransfersManagerObservable();

    virtual ~FileTransfersManagerObservable();

    virtual void notifyObserversTransferBeginRequest(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize);
    virtual void notifyObserversTransferBegin(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize);
    virtual void notifyObserverTransferData(uint64_t pId, uint64_t pTransferredSize);

    //virtual void notifyObservers(GeneralEvent *pEvent);
    virtual void AddObserver(FileTransfersManagerObserver *pObserver);
    virtual void DeleteObserver(FileTransfersManagerObserver *pObserver);

private:
    FileTransfersManagerObserver     *mFileTransfersManagerObserver;
};

///////////////////////////////////////////////////////////////////////////////
class FileTransfer;
typedef std::vector<FileTransfer*>  FileTransfers;

class FileTransfersManager:
    public FileTransfersManagerObservable,
    public Thread
{
public:
    FileTransfersManager();

    virtual ~FileTransfersManager();

    static FileTransfersManager& getInstance();

    void Init(std::string pLocalName, Requirements *pTransportRequirements);
    uint64_t SendFile(std::string pTargetName, Requirements *pTransportRequirements, std::string pFileName); // returns ID
    void AcknowledgeTransfer(uint64_t pId, std::string pLocalFileName);
    void PauseTransfer(uint64_t pId);
    void ContinueTransfer(uint64_t pId);
    void CancelTransfer(uint64_t pId);
    // GetOutgoingTransfers();
    // GetIncomingTransfers();

    unsigned int GetPort();
    uint32_t GetSourceId();
    static uint32_t Id2SourceId(uint64_t pId);
    bool IsFromLocalhost(uint64_t pId) ;

private:
    friend class FileTransfer;

    /* send/receive transfer database */
    void RegisterTransfer(FileTransfer* pInstance);
    FileTransfer* SearchTransfer(uint64_t pId);
    //void UnregisterTransfer(FileTransfer* pInstance); //TODO
    FileTransfers       mFileTransfers;
    Mutex               mFileTransfersMutex;

    /* main receiver */
    void StartListener();
    void StopListener();
    bool ReceivePacket(std::string &pSourceHost, unsigned int &pSourcePort, char* pData, int &pSize);
    virtual void* Run(void* pArgs = NULL);

    bool                mFileListenerRunning;
    bool                mFileListenerNeeded;
    char                *mPacketReceiveBuffer;
    int                 mReceiveErrors;
    unsigned int        mLocalPort;

    /* global sender values */
    unsigned int        mSourceId;

    /* NAPI based transport */
    IConnection         *mReceiverSocket;
    IBinding            *mNAPIBinding;
    std::string         mLocalName;
    Requirements        *mTransportRequirements;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
