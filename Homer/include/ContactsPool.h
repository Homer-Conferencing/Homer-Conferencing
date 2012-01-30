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
 * Purpose: Data pool for contacts management
 * Author:  Thomas Volkert
 * Since:   2009-04-07
 */

#ifndef _CONTACTS_POOL_
#define _CONTACTS_POOL_

#include <Widgets/OverviewContactsWidget.h>

#include <QAbstractItemModel>
#include <QMutex>

#include <vector>
#include <string>

#define DATABASE_IDENTIFIER "Homer_database"

namespace Homer { namespace Gui {

#define         CONTACT_UNDEFINED_STATE         -1
#define         CONTACT_UNAVAILABLE             0
#define         CONTACT_AVAILABLE               1

struct ContactDescriptor
{
    QString        Name;
    QString        User;
    QString        Host;
    QString        Port;
    int            State; // 0-offline, 1-online/available
    unsigned int   Id;

    QString     toString();
    std::string getUserStdStr();
    std::string getHostStdStr();
    std::string getPortStdStr();
    void        setUserStdStr(std::string pUser);
    void        setHostStdStr(std::string pHost);
    bool        isOnline();
};

typedef std::vector<ContactDescriptor> ContactsVector;

// forward declaration
class ContactListModel;

///////////////////////////////////////////////////////////////////////////////

#define CONTACTSPOOL ContactsPool::getInstance()

///////////////////////////////////////////////////////////////////////////////

class ContactsPool
{
public:
    ContactsPool();

    virtual ~ContactsPool();

    static ContactsPool& getInstance();

    void Init(const std::string& pContactsFile);
    std::string getContactsFilePath();

    void SavePool(std::string pContactsFile = "");
    void LoadPool(std::string pContactsFile = "");

    int GetNextFreeId();
    bool FavorizedContact(QString pUser, QString pHost, QString pPort);


    /* pool manipulation */
    void AddContact(ContactDescriptor &pContact);
    void RemoveContact(unsigned int pId);

    /* queries */
    ContactDescriptor* GetContactById(unsigned int pId);
    ContactDescriptor* GetContactByIndex(unsigned int pIndex);
    bool IsKnownContact(QString pUser, QString pHost, QString pPort);
    int ContactCount();

    /* helper */
    ContactDescriptor* CreateContactDescriptor(QString pUser, QString pHost, QString pPort = "");
    bool SplitAddress(QString pAddr, QString &pUser, QString &pHost, QString &pPort);

    /* contact availability management */
    void ProbeAvailabilityForAll();
    void UpdateContactState(QString pContact, bool pState);

    /* sorting */
    void SortByState(bool pDescending);
    void SortByName(bool pDescending);
    void UpdateSorting();

    void RegisterAtController(ContactListModel *pContactsModel);

private:
    ContactListModel    *mContactsModel;
    ContactsVector      mContacts;
    std::string         mContactsFile;
    QMutex              mContactsMutex;
    bool                mSortByState;
    bool                mSortDescending;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
