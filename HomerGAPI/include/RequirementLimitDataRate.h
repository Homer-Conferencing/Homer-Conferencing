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
 * Purpose: RequirementLimitDataRate
 * Author:  Thomas Volkert
 * Since:   2011-12-10
 */

#ifndef _GAPI_REQUIREMENT_LIMIT_DATARATE_
#define _GAPI_REQUIREMENT_LIMIT_DATARATE_

#include <IRequirement.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class RequirementLimitDataRate:
    public TRequirement<RequirementLimitDataRate, REQUIREMENT_LIMIT_DATARATE>
{
public:
    RequirementLimitDataRate(int pMinDataRate, int pMaxDataRate):mMinDataRate(pMinDataRate), mMaxDataRate(pMaxDataRate){}

    virtual std::string getDescription(){ return "Requ(LimitDataRate[" + toString(mMinDataRate) + "," + toString(mMaxDataRate) + "])"; }

    int getMinDataRate(){ return mMinDataRate; }
    int getMaxDataRate(){ return mMaxDataRate; }
private:
    int     mMinDataRate, mMaxDataRate;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
