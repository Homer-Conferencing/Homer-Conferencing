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
 * Purpose: Implementation for presence information data format (RFC 3863)
 * Author:  Thomas Volkert
 * Since:   2010-11-04
 */

#include <Header_SofiaSip.h>
#include <PIDF.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Conference {

using namespace std;

using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

PIDF::PIDF()
{
}

PIDF::~PIDF()
{
}

///////////////////////////////////////////////////////////////////////////////
string PIDF::GetMimeFormatPidf()
{
	return "application/pidf+xml";
}

sip_payload_t* PIDF::CreatePresenceInPidf(su_home_t* pHome, string pUsername, string pServer, string pStateNote, bool pAcceptInstantMessages)
{
    sip_payload_t *tResult = NULL;
    tResult = sip_payload_format(pHome,
									   "<?xml version='1.0' encoding='UTF-8'?>\n"
									   "<presence xmlns='urn:ietf:params:xml:ns:pidf' entity='pres:%s'>\n"
									   "  <tuple id='sip:%s'>\n"
									   "    <status>\n"
									   "      <basic>%s</basic>\n"
									   "    </status>\n"
									   "    <note>%s</note>\n"
									   "  </tuple>\n"
									   "</presence>\n",
								   (pUsername + "@" + pServer).c_str(),
								   (pUsername + "@" + pServer).c_str(),
								   pAcceptInstantMessages ? "open" : "closed",
								   pStateNote.c_str());

    if (tResult != NULL)
    	LOG(LOG_VERBOSE, "PIDF based presence representation: %s", tResult->pl_data);
    else
    	LOG(LOG_ERROR, "Failed to create presence description");

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
