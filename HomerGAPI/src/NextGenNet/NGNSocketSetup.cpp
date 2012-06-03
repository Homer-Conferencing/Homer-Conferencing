/*****************************************************************************
 *
 * Copyright (C) 2012 Martin Becke
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
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#include <GAPI.h>
#include <NextGenNet/NGNSocketSetup.h>
#include <NextGenNet/NGNSocketBinding.h>
#include <NextGenNet/NGNSocketConnection.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////
/**
 * Constructor
 */
NGNSocketSetup::NGNSocketSetup()
{
}
/**
 * De-Constructor
 */
NGNSocketSetup::~NGNSocketSetup()
{
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Just a wrapper method to setup a connection
 */
IConnection* NGNSocketSetup::connect(Name *pName, Requirements *pRequirements)
{
    return new NGNSocketConnection(pName->toString(), pRequirements);
}

/**
 * Just a wrapper method to bind to a socket
 */
IBinding* NGNSocketSetup::bind(Name *pName, Requirements *pRequirements)
{
    return new NGNSocketBinding(pName->toString(), pRequirements);
}

/**
 * TODO
 */
Requirements NGNSocketSetup::getCapabilities(Name *pName, Requirements *pImportantRequirements)
{
    Requirements tResult;

    //TODO:

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
