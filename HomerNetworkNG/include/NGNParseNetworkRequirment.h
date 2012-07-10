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
 * Purpose: NGNParseRequirment
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#ifndef _GAPI_NGNPARSE_NETWORK_REQUIRMENT_
#define _GAPI_NGNPARSE_NETWORK_REQUIRMENT_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <NGNSocketBinding.h>
#include <NGNSocketConnection.h>

#include <HBSocketQoSSettings.h>
#include <Requirements.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitStream.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementTargetPort.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>

#define FIRST_CALL_REQUIRMENT    0
#define RECALL_REQUIRMENT        1

#define NOT_A_VALID_CLASS       -1


namespace Homer { namespace Base {    
///////////////////////////////////////////////////////////////////////////////
/**
 * Just a helper class to generate msgs for frogger
 */
class NGNParseNetworkRequirment
{
public:
   
    static int parse(NGNSocketBinding* tmp, int kind=FIRST_CALL_REQUIRMENT){
        Requirements r = (tmp->getRequirements());
        return parseAndSignal(&r,kind);
    }
    static int parse(NGNSocketConnection* tmp,int kind=FIRST_CALL_REQUIRMENT){
        Requirements r = (tmp->getRequirements());
        return parseAndSignal(&r,kind);
    }                        
                              
    static int parseAndSignal(Requirements* r = NULL,int kind=FIRST_CALL_REQUIRMENT){
       if(r==NULL)
            return NOT_A_VALID_CLASS;
        
        
        // Parse it like thomas it did
        bool tResult = false;
        
        /* additional transport requirements */
        if (r->contains(RequirementTransmitBitErrors::type()))
        {
            RequirementTransmitBitErrors* tReqBitErr = (RequirementTransmitBitErrors*)r->get(RequirementTransmitBitErrors::type());
            int tSecuredFrontDataSize = tReqBitErr->getSecuredFrontDataSize();
 //           mSocket->UDPLiteSetCheckLength(tSecuredFrontDataSize);
        }
        
        /* QoS requirements */
        int tMaxDelay = 0;
        int tMinDataRate = 0;
        int tMaxDataRate = 0;
        // get lossless transmission activation
        bool tLossless = r->contains(RequirementTransmitLossless::type());
        // get delay values
        if(r->contains(RequirementLimitDelay::type()))
        {
            RequirementLimitDelay* tReqDelay = (RequirementLimitDelay*)r->get(RequirementLimitDelay::type());
            tMaxDelay = tReqDelay->getMaxDelay();
        }
        // get data rate values
        if(r->contains(RequirementLimitDataRate::type()))
        {
            RequirementLimitDataRate* tReqDataRate = (RequirementLimitDataRate*)r->get(RequirementLimitDataRate::type());
            tMinDataRate = tReqDataRate->getMinDataRate();
            tMaxDataRate = tReqDataRate->getMaxDataRate();
        }
        
        if((tLossless) || (tMaxDelay) || (tMinDataRate))
        {
            QoSSettings tQoSSettings;
            tQoSSettings.DataRate = tMinDataRate;
            tQoSSettings.Delay = tMaxDelay;
            tQoSSettings.Features = (tLossless ? QOS_FEATURE_LOSSLESS : 0);
//            tResult = mSocket->SetQoS(tQoSSettings);
        }
        
//        mRequirements = *pRequirements; //TODO: maybe some requirements were dropped?
        return 0;

    };
};
///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
