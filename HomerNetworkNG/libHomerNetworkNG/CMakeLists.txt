###############################################################################
# Author:  Thomas Volkert
# Since:   2012-06-10
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeConfig.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# include dirs
SET (INCLUDE_DIRS
    ../include
	../../HomerBase/include
	../../HomerBase/include/Logging
	../../HomerGAPI/include
	../../HomerMonitor/include
)

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
# compile flags
SET (FLAGS
	${FLAGS}
	
)

##############################################################
# SOURCES
SET (SOURCES
	../src/NGNSocketName
	../src/NGNSocketSetup
    	../src/NGNSocketBinding
    	../src/NGNSocketConnection
	../../HomerGAPI/src/Requirements
	../../HomerGAPI/src/Events
	../../HomerGAPI/src/Berkeley/SocketName
)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS_INSTALL
    libgcc_s_dw2-1
    libstdc++-6
)
SET (LIBS_WINDOWS
	HomerBase
)
	
# USED LIBRARIES for BSD environment
SET (LIBS_BSD
	HomerBase
)

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
	sctp
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	sctp
)

##############################################################
SET (TARGET_LIB_NAME
	HomerNetworkNG
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)