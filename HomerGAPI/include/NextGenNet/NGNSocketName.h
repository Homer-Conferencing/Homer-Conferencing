/*****************************************************************************
 *
 * Copyright (C) 2012 Martin.Becke@uni-due.de
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
 * Purpose: SocketName
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#ifndef _GAPI_NGNSOCKET_NAME_
#define _GAPI_NGNSOCKET_NAME_

#include <Name.h>
#include <string>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class NGNSocketName:
    public Name
{
public:
    NGNSocketName(std::string pHost, unsigned int pPort);
    virtual ~NGNSocketName();

    virtual std::string toString();
    virtual std::string getHost();
    virtual unsigned int getPort();
private:
    std::string		mHost;
    unsigned int    mPort;
    std::string     mTransport;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
