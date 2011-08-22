/*
 * Name:    Header_Alsa.h
 * Purpose: header includes for alsa library
 * Author:  Thomas Volkert
 * Since:   2009-06-05
 * Version: $Id: Header_Alsa.h 11 2011-08-22 13:15:28Z silvo $
 */

#ifndef _SOUNDOUT_HEADER_ALSA_
#define _SOUNDOUT_HEADER_ALSA_

#ifdef LINUX
#pragma GCC system_header //suppress warnings from alsa

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#endif
#endif
