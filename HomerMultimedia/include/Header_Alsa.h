/*
 * Name:    Header_Alsa.h
 * Purpose: header includes for alsa library
 * Author:  Thomas Volkert
 * Since:   2009-06-05
 * Version: $Id: Header_Alsa.h,v 1.5 2011/02/19 13:53:35 chaos Exp $
 */

#ifndef _MULTIMEDIA_HEADER_ALSA_
#define _MULTIMEDIA_HEADER_ALSA_

#ifdef LINUX
#pragma GCC system_header //suppress warnings from alsa

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#endif
#endif
