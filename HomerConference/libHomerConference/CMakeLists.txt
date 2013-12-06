###############################################################################
# Author:  Thomas Volkert
# Since:   2011-02-19
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeConfig.txt)

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
	../../HomerMultimedia/include
	/usr/include/sofia-sip
	/usr/include/sofia-sip-1.12
	/usr/local/include/sofia-sip
	/usr/local/include/sofia-sip-1.12
    ../../../Homer-Release/HomerIncludes/sofia-sip/
	/usr/include/ffmpeg
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
IF (NOT (WINDOWS))
SET (TARGET_DIRECTORY
    ${RELOCATION_DIR}lib
)
ELSE (NOT (WINDOWS))
SET (TARGET_DIRECTORY
    ${RELOCATION_DIR}
)
ENDIF (NOT (WINDOWS))

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
    libgcc_s_dw2-1.dll
    libsofia_sip_ua.dll
    libstdc++-6.dll
    msvcr100.dll
    pthreadVC2.dll
)
SET (LIBS_WINDOWS
	HomerBase
	HomerMonitor
	HomerMultimedia
	ws2_32
    libsofia_sip_ua
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerMonitor
	HomerMultimedia
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
		HomerMultimedia
		sofia-sip-ua
		${OPENSSL_LIBRARIES}
	)
ELSE ()
	# USED LIBRARIES for linux environment
	SET (LIBS_LINUX
		HomerBase
		HomerMonitor
		HomerMultimedia
		dl
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
	HomerMultimedia
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
