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
 * Purpose: presence information data format (RFC 3863)
 * Since:   2010-11-04
 */

#ifndef _CONFERENCE_PIDF_
#define _CONFERENCE_PIDF_

#include <Header_SofiaSipForwDecl.h>
#include <string>

namespace Homer { namespace Conference {

///////////////////////////////////////////////////////////////////////////////

class PIDF
{
public:
	PIDF();

    virtual ~PIDF();

protected:
    std::string GetMimeFormatPidf();
    sip_payload_t* CreatePresenceInPidf(su_home_t* pHome, std::string pUsername, std::string pServer, std::string pStatusNote = "online", bool pAcceptInstantMessages = true);
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
