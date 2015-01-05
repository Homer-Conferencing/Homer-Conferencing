/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: header includes for sofia-sip library
 * Since:   2009-02-16
 */

#ifndef _CONFERENCE_HEADER_SOFIASIP_
#define _CONFERENCE_HEADER_SOFIASIP_

#if defined(WINDOWS)
#include <stdio.h>
#endif

#pragma GCC system_header //suppress warnings from sofia-sip

#include <sofia-sip/msg_addr.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/nua_tag.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/soa.h>
#include <sofia-sip/stun_tag.h>
#include <sofia-sip/su.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_io.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_wait.h>
#include <sofia-sip/stun.h>
#include <sofia-sip/stun_tag.h>
#include <sofia-sip/tport_tag.h>
#include <sofia-sip/url.h>
#include <sofia-sip/sofia_features.h>

#endif
