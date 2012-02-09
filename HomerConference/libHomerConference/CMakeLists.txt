###############################################################################
# Author:  Thomas Volkert
# Since:   2011-02-19
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

MESSAGE("#####################################")
MESSAGE("Configure HomerConference")
MESSAGE("#####################################")

##############################################################
# Configuration
##############################################################

# include dirs
SET (INCLUDE_DIRS
	../include
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerMonitor/include
	/usr/include/sofia-sip
	/usr/include/sofia-sip-1.12
	/usr/local/include/sofia-sip
	/usr/local/include/sofia-sip-1.12
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/bnf
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/features
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/http
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/ipt
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/iptsec
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/msg
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/nea
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/nta
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/nth
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/nua
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/sdp
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/sip
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/soa
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/sresolv
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/stun
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/su
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/tport
	../../HomerWinLibs/SOFIA-SIP/libsofia-sip-ua/url
	../../HomerWinLibs/SOFIA-SIP/win32
)

# compile flags                                                                                                                                                                                                                                                                   
if (NOT APPLE)
SET (FLAGS
    ${FLAGS}
    -Wno-uninitialized
)
endif (NOT APPLE)

# target directory for the lib
SET (TARGET_DIRECTORY
	${RELOCATION_DIR}../../lib
)

# SOURCES
SET (SOURCES
	../src/Meeting
	../src/MeetingEvents
	../src/PIDF
	../src/SDP
	../src/SIP
	../src/SIP_stun
)

# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	HomerBase
	HomerMonitor
	ws2_32
	libsofia_sip_ua
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerMonitor
)

# USED static LIBRARIES for apple environment
SET (LIBS_APPLE_STATIC
	sofia-sip-ua
	ssl
	crypto
)

# USED FRAMEWORKS for apple environment
SET (FRAMEWORKS_APPLE
	CoreFoundation
	SystemConfiguration
)

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
	HomerMonitor
)
SET (LIBS_LINUX_STATIC
	sofia-sip-ua
	ssl
	crypto
)

# USED LIBRARIES for BSD environment
SET (LIBS_BSD
    HomerBase
    HomerMonitor
)
SET (LIBS_BSD_STATIC
    sofia-sip-ua
    ssl
    crypto
)

SET (TARGET_NAME
	HomerConference
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)