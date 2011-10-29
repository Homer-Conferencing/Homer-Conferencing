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
 * Purpose: forward declaration of some basic sofia-sip types to avoid direct dependency from sofia-sip headers
 * Author:  Thomas Volkert
 * Since:   2011-10-28
 */

#ifndef _CONFERENCE_HEADER_SOFIASIP_FORW_DECL
#define _CONFERENCE_HEADER_SOFIASIP_FORW_DECL

struct nua_handle_s;
typedef struct nua_handle_s nua_handle_t;

struct stun_discovery_s;
typedef struct stun_discovery_s  stun_discovery_t;

struct sip_addr_s;
typedef struct sip_addr_s           sip_to_t;

/** Structure for accessing parsed SIP headers. */
struct sip_s;
typedef struct sip_s                sip_t;

struct msg_payload_s;
typedef struct msg_payload_s        msg_payload_t;
typedef msg_payload_t               sip_payload_t;

/** NUA agent. */
struct nua_s;
typedef struct nua_s nua_t;

struct su_root_s;
typedef struct su_root_s su_root_t;

#ifndef SU_HOME_T
#define SU_HOME_T struct su_home_s
/** Memory home type. */
typedef SU_HOME_T su_home_t;
#endif

#ifndef NUA_MAGIC_T
#define NUA_MAGIC_T void
/** Application context for NUA agent. */
typedef NUA_MAGIC_T nua_magic_t;
#endif

#ifndef NUA_HMAGIC_T
#define NUA_HMAGIC_T void
/** Application context for NUA handle. */
typedef NUA_HMAGIC_T nua_hmagic_t;
#endif

#endif
