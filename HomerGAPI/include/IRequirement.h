/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Purpose: IRequirements
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#ifndef _GAPI_IREQUIREMENT_
#define _GAPI_IREQUIREMENT_

#include <Requirements.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

// network attributes
#define REQUIREMENT_USE_IPv6                            0x0006

// transport attributes
#define REQUIREMENT_TRANSMIT_LOSLESS                    0x0101
#define REQUIREMENT_TRANSMIT_CHUNKS                     0x0102
#define REQUIREMENT_TRANSMIT_WATERFALL                  0x0103

// additional transport attributes
#define REQUIREMENT_TRANSMIT_BIT_ERRORS                 0x0111
#define REQUIREMENT_TRANSMIT_FAST                       0x0112

// QoS/throughput attributes
#define REQUIREMENT_LIMIT_DELAY                         0x0201
#define REQUIREMENT_LIMIT_DATARATE                      0x0202

///////////////////////////////////////////////////////////////////////////////

// forward declaration
class Requirements;

class IRequirement
{
public:
	IRequirement(int pType):mType(pType)
	{

	}
    virtual ~IRequirement( )
    {

    }

    virtual std::string getDescription() = 0;
    virtual int getType()const
    {
        return mType;
    }

private:
    int mType;
};

template <typename DerivedClass, int pType>
class TRequirement:
    public IRequirement
{
public:
    TRequirement():IRequirement(pType)
    {

    }
    virtual ~TRequirement()
    {

    }

    static int type()
    {
        return pType;
    }
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
