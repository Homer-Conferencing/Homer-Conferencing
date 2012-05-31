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
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <GAPI.h>
#include <Core/ChannelName.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

ChannelName::ChannelName(string pNode, unsigned int pPort):
	Name(pNode), mNode(pNode), mPort(pPort)
{

}

ChannelName::~ChannelName()
{

}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
inline std::string dataToString(T const& value_)
{
    std::stringstream ss;
    ss << value_;
    return ss.str();
}

string ChannelName::toString()
{
	return mNode + "<" + dataToString(mPort) + ">";
}

std::string ChannelName::getNode()
{
    return mNode;
}

unsigned int ChannelName::getPort()
{
    return mPort;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
