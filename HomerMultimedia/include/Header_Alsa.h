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
 * Name:    Header_Alsa.h
 * Purpose: header includes for alsa library
 * Author:  Thomas Volkert
 * Since:   2009-06-05
 * Version: $Id$
 */

#ifndef _MULTIMEDIA_HEADER_ALSA_
#define _MULTIMEDIA_HEADER_ALSA_

#ifdef LINUX
#pragma GCC system_header //suppress warnings from alsa

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#endif
#endif
