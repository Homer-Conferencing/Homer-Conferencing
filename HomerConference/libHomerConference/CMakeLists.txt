###############################################################################
# Author:  Thomas Volkert
# Since:   2011-02-19
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# find package "OpenSSL"
IF (LINUX)
MESSAGE("##### Searching for library OpenSSL")
Find_Package(OpenSSL REQUIRED)
IF (NOT OPENSSL_FOUND)
	MESSAGE(FATAL_ERROR "You need library OpenSSL")
ENDIF(NOT OPENSSL_FOUND)
ELSE (LINUX)
    SET(OPENSSL_INCLUDE_DIR "/usr/include")
ENDIF (LINUX)

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
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/bnf
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/features
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/http
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/ipt
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/iptsec
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/msg
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/nea
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/nta
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/nth
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/nua
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/sdp
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/sip
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/soa
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/sresolv
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/stun
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/su
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/tport
	../../../Homer-Release/HomerIncludes/sofia-sip/libsofia-sip-ua/url
	../../../Homer-Release/HomerIncludes/sofia-sip/win32
	${OPENSSL_INCLUDE_DIR}
)

##############################################################
# compile flags                                                                                                                                                                                                                                                                   
if (NOT APPLE)
SET (FLAGS
	${FLAGS}
	-Wno-uninitialized
)
endif (NOT APPLE)

##############################################################
# target directory for the lib
IF (NOT (WIN32))
SET (TARGET_DIRECTORY
    ${RELOCATION_DIR}lib
)
ELSE (NOT (WIN32))
SET (TARGET_DIRECTORY
    ${RELOCATION_DIR}
)
ENDIF (NOT (WIN32))

##############################################################
# SOURCES
SET (SOURCES
	../src/Meeting
	../src/MeetingEvents
	../src/PIDF
	../src/SDP
	../src/SIP
	../src/SIP_stun
)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS_INSTALL
    libgcc_s_dw2-1
    libsofia_sip_ua
    libstdc++-6
    msvcr100
    pthreadVC2
)
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

IF (${BUILD} MATCHES "Default")
	SET (LIBS_LINUX
		HomerBase
		HomerMonitor
		sofia-sip-ua
		${OPENSSL_LIBRARIES}
	)
ELSE ()
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
ENDIF ()

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

##############################################################
SET (TARGET_LIB_NAME
	HomerConference
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)