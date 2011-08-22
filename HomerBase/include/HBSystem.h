/*
 * Name:    System.h
 * Purpose: header for system specific and unspecific basic functions
 * Author:  Thomas Volkert
 * Since:   2011-02-25
 * Version: $Id$
 */

#ifndef _BASE_SYSTEM_
#define _BASE_SYSTEM_

#include <string.h>

#ifdef LINUX
#endif

#ifdef WIN32
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class System
{
public:
    System( );

    virtual ~System( );

    static bool GetWindowsKernelVersion(int &pMajor, int &pMinor);
    static std::string GetLinuxKernelVersion();
private:
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
