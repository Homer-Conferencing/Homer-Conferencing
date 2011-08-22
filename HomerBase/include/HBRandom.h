/*
 * Name:    Mutex.h
 * Purpose: wrapper for os independent random number generator
 * Author:  Thomas Volkert
 * Since:   2010-09-28
 * Version: $Id: HBRandom.h 6 2011-08-22 13:06:22Z silvo $
 */

#ifndef _BASE_RANDOM_
#define _BASE_RANDOM_

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class Random
{
public:
	Random( );

    virtual ~Random( );
    static unsigned long GenerateNumber();
private:
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
