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
 * Purpose: Implementation of data pool for contacts management
 * Author:  Thomas Volkert
 * Since:   2009-04-07
 */

#include <string>
#include <fstream>
#include <exception>

#include <ContactsManager.h>
#include <Configuration.h>
#include <Logger.h>
#include <Meeting.h>

#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QFile>
#include <QIODevice>
#include <QTextStream>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Conference;

ContactsManager sContactsManager;

///////////////////////////////////////////////////////////////////////////////

ContactsManager::ContactsManager()
{
    mContactsModel = NULL;
    mContactsFile = "";
}

ContactsManager::~ContactsManager()
{
}

///////////////////////////////////////////////////////////////////////////////

ContactsManager& ContactsManager::getInstance()
{
    return (sContactsManager);
}

void ContactsManager::Init(const std::string& pContactsFile)
{
    LOG(LOG_VERBOSE, "Initializing contacts..");

    mContactsFile = pContactsFile;
    LoadPool();
}

string ContactsManager::getContactsFilePath()
{
    return mContactsFile;
}

void ContactsManager::SavePool(string pContactsFile)
{
    // should we use an old contacts file?
    if (pContactsFile == "")
    {
        if (mContactsFile != "")
            pContactsFile = mContactsFile;
        else
            return;
    }else
        mContactsFile = pContactsFile;

    QDomDocument tXml(DATABASE_IDENTIFIER);

    //###################################################
    //### create root node
    //###################################################
    QDomElement tRoot = tXml.createElement("contacts");
    tXml.appendChild(tRoot);

    //###################################################
    //### create element for every contact
    //###################################################
    ContactsVector::iterator tIt;
    mContactsMutex.lock();
    for (tIt = mContacts.begin(); tIt != mContacts.end(); tIt++)
    {
        QDomElement tEntry = tXml.createElement("entry");

        tEntry.setAttribute("Name", QString(tIt->Name.toAscii()));
        tEntry.setAttribute("User", QString(tIt->User.toAscii()));
        tEntry.setAttribute("Host", QString(tIt->Host.toAscii()));
        tEntry.setAttribute("Port", QString(tIt->Port.toAscii()));
        tEntry.setAttribute("Transport", QString(QString(Socket::TransportType2String(tIt->Transport).c_str()).toAscii()));
        tEntry.setAttribute("Index", QString("%1").arg(tIt->Id));

        tRoot.appendChild(tEntry);
    }
    mContactsMutex.unlock();

    //###################################################
    //### open XML file
    //###################################################
    QFile tFile(QString(pContactsFile.c_str()));
    if (!tFile.open(QIODevice::WriteOnly))
    {
        return;
    }

    //###################################################
    //### write to XML file
    //###################################################
    QTextStream tTs(&tFile);
    tTs << tXml.toString();
    tFile.close();

    UpdateSorting();
    CONF.SetContactFile(tFile.fileName());
}

void ContactsManager::LoadPool(string pContactsFile)
{
    // should we use an old contacts file?
    if (pContactsFile == "")
    {
        if (mContactsFile != "")
            pContactsFile = mContactsFile;
        else
            return;
    }else
        mContactsFile = pContactsFile;

	QDomDocument tXml(DATABASE_IDENTIFIER);
	QFile tFile(QString(pContactsFile.c_str()));

	ResetPool();

	//###################################################
	//### open XML file
	//###################################################
	if (!tFile.open(QIODevice::ReadOnly))
		return;

	//###################################################
	//### assign XML file to DOM document
	//###################################################
	QString tErrBuffer;
	if (!tXml.setContent(&tFile, false, &tErrBuffer))
	{
		tFile.close();
		LOG(LOG_ERROR, "Unable to assign XML file because of \"%s\"", tErrBuffer.toStdString().c_str());
		return;
	}

	//###################################################
	//### get root element
	//###################################################
	QDomElement tRoot = tXml.documentElement();
	if (tRoot.tagName() != "contacts")
	{
		tFile.close();
		LOG(LOG_ERROR, "Name of root element doesn't match the database identifier");
		return;
	}

	//###################################################
	//### parse entry per entry
	//###################################################
    mContactsMutex.lock();
	QDomNode tXmlNode = tRoot.firstChild();
	while (!tXmlNode.isNull())
	{
		QDomElement tEntry = tXmlNode.toElement();
		if (!tEntry.isNull())
		{
			if (tEntry.tagName() == "entry")
			{
				ContactDescriptor tContact;

				tContact.User = QString::fromAscii(tEntry.attribute("User", "User").toStdString().c_str());
                tContact.Name = QString::fromAscii(tEntry.attribute("Name", tContact.User).toStdString().c_str()); // per default use the user name as contact name
				tContact.Host = QString::fromAscii(tEntry.attribute("Host", "Host").toStdString().c_str());
				tContact.Host = tContact.Host.toLower();
				tContact.Port = QString::fromAscii(tEntry.attribute("Port", "5060").toStdString().c_str());
				tContact.Transport = Socket::String2TransportType(QString::fromAscii(tEntry.attribute("Transport", "UDP").toStdString().c_str()).toStdString());
				LOG(LOG_VERBOSE, "Loaded contact: name=%s, address=%s, port=%s, transport=%s", QString(tContact.Name.toAscii()).toStdString().c_str(), QString(tContact.User.toAscii() + "@" + tContact.Host.toAscii()).toStdString().c_str(), tContact.Port.toStdString().c_str(), Socket::TransportType2String(tContact.Transport).c_str());
				tContact.Id = tEntry.attribute("Index", "0").toUInt();
				tContact.State = CONTACT_UNAVAILABLE;

				mContacts.push_back(tContact);
			}else
				LOG(LOG_ERROR, "Incompatible entry tag name");
		}
		tXmlNode = tXmlNode.nextSibling();
	}
    mContactsMutex.unlock();

	mContactsFile = pContactsFile;
	ProbeAvailabilityForAll();
    UpdateSorting();
	if (mContactsModel != NULL)
        mContactsModel->UpdateView();
    CONF.SetContactFile(tFile.fileName());
}

void ContactsManager::ResetPool()
{
    mContactsMutex.lock();
    mContacts.clear();
    mContactsMutex.unlock();

    if (mContactsModel != NULL)
        mContactsModel->UpdateView();
}

void ContactsManager::AddContact(ContactDescriptor &pContact)
{
    pContact.State = CONTACT_UNAVAILABLE;

    mContactsMutex.lock();
    mContacts.push_back(pContact);
    if (CONF.GetSipContactsProbing())
        MEETING.SendProbe(MEETING.SipCreateId(pContact.getUserStdStr(), pContact.getHostStdStr(), pContact.getPortStdStr()), pContact.Transport);
    mContactsMutex.unlock();
    if (mContactsModel != NULL)
        mContactsModel->UpdateView();

    SavePool();
}

void ContactsManager::RemoveContact(unsigned int pId)
{
    mContactsMutex.lock();
    ContactsVector::iterator tIt, tItEnd = mContacts.end();

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        if (tIt->Id == pId)
        {
            mContacts.erase(tIt);

            mContactsMutex.unlock();

            if (mContactsModel != NULL)
                mContactsModel->UpdateView();

            SavePool();

            return;
        }
    }
    mContactsMutex.unlock();
}

bool ContactsManager::IsKnownContact(QString pUser, QString pHost, QString pPort)
{
    bool tFound = false;

    mContactsMutex.lock();
    ContactsVector::iterator tIt, tItEnd = mContacts.end();

    if (pHost != CONF.GetSipServer())
    	pUser = "";

    LOG(LOG_VERBOSE, "Searching in contact pool the contact: %s@%s<%s>", pUser.toStdString().c_str(), pHost.toStdString().c_str(), pPort.toStdString().c_str());

    bool tContactHasDefaultPort = false;
    if ((pPort == "5060") || (pPort == ""))
    	tContactHasDefaultPort = true;

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        LOG(LOG_VERBOSE, "Comparing %s==%s, %s==%s, %s==%s", tIt->User.toStdString().c_str(), pUser.toStdString().c_str(), tIt->Host.toStdString().c_str(), pHost.toStdString().c_str(), tIt->Port.toStdString().c_str(), pPort.toStdString().c_str());
        bool tEntryHasDefaultPort = false;
        if ((tIt->Port == "5060") || (tIt->Port == ""))
        	tEntryHasDefaultPort = true;

        if (((tIt->User == pUser)  || (pUser == "")) && ((tIt->Host == pHost)  || (pHost == "")) && ((tIt->Port == pPort) || ((tContactHasDefaultPort) && (tEntryHasDefaultPort))))
        {
            LOG(LOG_VERBOSE, "..found");
            tFound =true;
            break;
        }
    }
    mContactsMutex.unlock();

    return tFound;
}

int ContactsManager::ContactCount()
{
    int tResult;
    mContactsMutex.lock();
    tResult = mContacts.size();
    mContactsMutex.unlock();
    return tResult;

}

ContactDescriptor* ContactsManager::CreateContactDescriptor(QString pUser, QString pHost, QString pPort)
{
    ContactDescriptor *tResult = new ContactDescriptor();

    tResult->User = pUser;
    tResult->Host = pHost;
    tResult->Port = pPort;

    return tResult;
}

bool ContactsManager::SplitAddress(QString pAddr, QString &pUser, QString &pHost, QString &pPort)
{
    int tPos;

    tPos = pAddr.indexOf('@') -1;
    if(tPos >= 0)
        pUser = pAddr.left(pAddr.indexOf('@'));
    pAddr = pAddr.right(pAddr.size() - pAddr.indexOf('@') -1);

    // IPv6 ?
    if ((pAddr.contains('[')) || (pAddr.count(':') > 1))
    {
        tPos = pAddr.indexOf('[');

        // "[address]" ?
        if(tPos != -1)
        {
            pAddr = pAddr.section('[', 1, 1);
            pHost = pAddr.section(']', 0, 0);
            pPort = pAddr.section(']', 1, 1);
        }else
        {// "address"
            tPos = pAddr.lastIndexOf(':');
            pHost = pAddr.left(tPos);
            pPort = pAddr.right(pAddr.size() - tPos -1);
        }
    }else
    {
        pHost = pAddr.section(':', 0, 0);
        pPort = pAddr.section(':', 1, 1);
    }

    return true;
}

ContactDescriptor* ContactsManager::GetContactById(unsigned int pId)
{
    mContactsMutex.lock();

    ContactsVector::iterator tIt, tItEnd = mContacts.end();

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        if (tIt->Id == pId)
        {
            mContactsMutex.unlock();
            return &(*tIt);
        }
    }

    mContactsMutex.unlock();
    return NULL;
}

ContactDescriptor* ContactsManager::GetContactByIndex(unsigned int pIndex)
{
    ContactDescriptor *tResult = NULL;

    mContactsMutex.lock();

    if (mContacts.size() > pIndex)
        tResult = (&mContacts[pIndex]);

    mContactsMutex.unlock();

    return tResult;
}

bool ContactsManager::FavorizedContact(QString pUser, QString pHost, QString pPort)
{
    mContactsMutex.lock();

    bool tFound = false;
    ContactsVector::iterator tIt, tFoundIt, tItEnd = mContacts.end();

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        // have we found the contact within our internal contact pool?
        if ((tIt->User == pUser) && (tIt->Host == pHost) && (tIt-> Port == pPort))
        {
            tFoundIt = tIt;
            tFound = true;
            // search for the first occurrence
            break;
        }
    }

    if (tFound)
    {
        ContactsVector tContacts;

        unsigned int tNewIndex = 1;

        // rearrange the pool of contacts and adopt the id numbers
        tFoundIt->Id = tNewIndex;
        tContacts.push_back(*tFoundIt);
        for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
        {
            if (tIt != tFoundIt)
            {
                (*tIt).Id = ++tNewIndex;
                tContacts.push_back(*tIt);
            }
        }
        mContacts = tContacts;
    }

    mContactsMutex.unlock();

    if (mContactsModel != NULL)
        mContactsModel->UpdateView();

    // automatically save the new pool data
    SavePool();

    return tFound;
}

void ContactsManager::ProbeAvailabilityForAll()
{
    if (!CONF.GetSipContactsProbing())
        return;

    mContactsMutex.lock();

    ContactsVector::iterator tIt, tItEnd = mContacts.end();

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        MEETING.SendProbe(MEETING.SipCreateId(tIt->getUserStdStr(), tIt->getHostStdStr(), tIt->getPortStdStr()), tIt->Transport);
    }

    mContactsMutex.unlock();
}

void ContactsManager::UpdateContactState(QString pContact, enum TransportType pContactTransport, bool pState)
{
    mContactsMutex.lock();

    pContact = QString(pContact.toLocal8Bit());
    LOG(LOG_VERBOSE, "Updating availability state for %s to %d", pContact.toStdString().c_str(), pState);

    ContactsVector::iterator tIt, tItEnd = mContacts.end();

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        QString tItTransport = QString(Socket::TransportType2String(tIt->Transport).c_str());
        if (tItTransport == "auto")
            tItTransport = "UDP";
        QString tContactTransport = QString(Socket::TransportType2String(pContactTransport).c_str());
        if (tContactTransport == "auto")
            tContactTransport = "UDP";
        LOG(LOG_VERBOSE, "Comparing %s==%s, %s==%s", MEETING.SipCreateId(tIt->getUserStdStr(), tIt->getHostStdStr(), tIt->getPortStdStr()).c_str(), pContact.toStdString().c_str(), tItTransport.toStdString().c_str(), tContactTransport.toStdString().c_str());
        if ((MEETING.SipCreateId(tIt->getUserStdStr(), tIt->getHostStdStr(), tIt->getPortStdStr()) == pContact.toStdString()) && (tItTransport == tContactTransport))
        {
            tIt->State = pState;
            LOG(LOG_VERBOSE, " ..found and set state");
        }
    }

    mContactsMutex.unlock();

    if (mContactsModel != NULL)
        mContactsModel->UpdateView();
}

void ContactsManager::SortByState(bool pDescending)
{
    mSortDescending = pDescending;
    mSortByState = true;

    mContactsMutex.lock();

    LOG(LOG_VERBOSE, "Sorting contacts pool by state, descending = %d", pDescending);

    ContactsVector tNewDataBase;
    ContactsVector::iterator tIt, tItEnd, tItBestMatch;
    int tBestMatch = -1;

    while (mContacts.size() > 0)
    {
        tBestMatch = -1;
        tItEnd = mContacts.end();
        for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
        {
            int tCurEntry = tIt->State;
            //LOG(LOG_VERBOSE, "Current entry: %d (%s)", tCurEntry, tIt->User.c_str());

            if (tBestMatch == -1)
            {
                tBestMatch = tCurEntry;
                tItBestMatch = tIt;
            }else
            {
                if (((tCurEntry >= tBestMatch) && (!pDescending)) || ((tCurEntry <= tBestMatch) && (pDescending)))
                {
                    tBestMatch = tCurEntry;
                    tItBestMatch = tIt;
                }
            }
        }

        LOG(LOG_VERBOSE, "Picking entry %s, state %d", tItBestMatch->User.toStdString().c_str(), tItBestMatch->State);
        tNewDataBase.push_back((*tItBestMatch));
        mContacts.erase(tItBestMatch);
    }

    mContacts = tNewDataBase;

    mContactsMutex.unlock();

    if (mContactsModel != NULL)
        mContactsModel->UpdateView();
}

void ContactsManager::SortByName(bool pDescending)
{
    mSortDescending = pDescending;
    mSortByState = false;

    mContactsMutex.lock();

    LOG(LOG_VERBOSE, "Sorting contacts pool by name, descending = %d", pDescending);

    ContactsVector tNewDataBase;
    ContactsVector::iterator tIt, tItEnd, tItBestMatch;
    QString tBestMatch = "";

    while (mContacts.size() > 0)
    {
        tBestMatch = "";
        tItEnd = mContacts.end();
        for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
        {
            QString tCurEntry = tIt->Name;
            //LOG(LOG_VERBOSE, "Current entry: %s", tCurEntry.toStdString().c_str());

            if (tBestMatch == "")
            {
                tBestMatch = tCurEntry;
                tItBestMatch = tIt;
            }else
            {
                if (((tCurEntry >= tBestMatch) && (!pDescending)) || ((tCurEntry <= tBestMatch) && (pDescending)))
                {
                    tBestMatch = tCurEntry;
                    tItBestMatch = tIt;
                }
            }
        }

        //LOG(LOG_VERBOSE, "Picking entry %s, state %d", tItBestMatch->User.toStdString().c_str(), tItBestMatch->State);
        tNewDataBase.push_back((*tItBestMatch));
        mContacts.erase(tItBestMatch);
    }

    mContacts = tNewDataBase;

    mContactsMutex.unlock();

    if (mContactsModel != NULL)
        mContactsModel->UpdateView();
}

void ContactsManager::UpdateSorting()
{
    if (mSortByState)
        SortByState(mSortDescending);
    else
        SortByName(mSortDescending);
}

int ContactsManager::GetNextFreeId()
{
    mContactsMutex.lock();

    ContactsVector::iterator tIt, tItEnd = mContacts.end();
    unsigned int tMaxId = 0;

    for (tIt = mContacts.begin(); tIt != tItEnd; tIt++)
    {
        if (tIt->Id > tMaxId)
            tMaxId = tIt->Id;
    }

    mContactsMutex.unlock();

    return ++tMaxId;
}

void ContactsManager::RegisterAtController(ContactListModel *pContactsModel)
{
    mContactsModel = pContactsModel;
}

///////////////////////////////////////////////////////////////////////////////

QString ContactDescriptor::toString()
{
    QString tResult = "";

    if (Name != "")
        tResult += Name;
    else
        tResult += User + "@" + Host;

    return tResult;
}

string ContactDescriptor::getUserStdStr()
{
    return QString(User.toLocal8Bit()).toStdString();
}

string ContactDescriptor::getHostStdStr()
{
    return QString(Host.toLocal8Bit()).toStdString();
}

string ContactDescriptor::getPortStdStr()
{
    return QString(Port.toLocal8Bit()).toStdString();
}

void ContactDescriptor::setUserStdStr(string pUser)
{
    User = QString::fromAscii(pUser.c_str());
}

void ContactDescriptor::setHostStdStr(string pHost)
{
    User = QString::fromAscii(pHost.c_str());
}

bool ContactDescriptor::isOnline()
{
    return (State == CONTACT_AVAILABLE);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
