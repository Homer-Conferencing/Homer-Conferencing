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
 * Purpose: Implementation of G-Lab API
 * Since:   2011-12-08
 */

#include <NAPI.h>
#include <Berkeley/SocketSetup.h>
#include <Berkeley/SocketBinding.h>
#include <Berkeley/SocketConnection.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

SocketSetup::SocketSetup()
{
}

SocketSetup::~SocketSetup()
{

}

///////////////////////////////////////////////////////////////////////////////

IConnection* SocketSetup::connect(Name *pName, Requirements *pRequirements)
{
	return new SocketConnection(pName->toString(), pRequirements);
}

ICEPBinding* SocketSetup::bind(Name *pName, Requirements *pRequirements)
{
    return new SocketBinding(pName->toString(), pRequirements);
}

Requirements SocketSetup::getCapabilities(Name *pName, Requirements *pImportantRequirements)
{
	Requirements tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
