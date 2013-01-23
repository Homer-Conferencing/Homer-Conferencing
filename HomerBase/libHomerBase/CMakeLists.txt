###############################################################################
# Author:  Thomas Volkert
# Since:   2010-11-13
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeConfig.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# include dirs
SET (INCLUDE_DIRS
	../include
	../include/Logging
)

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
# compile flags
SET (FLAGS
	${FLAGS}
)

##############################################################
# SOURCES
SET (SOURCES
	../src/HBMutex
	../src/HBCondition
	../src/HBRandom
	../src/HBReflection
	../src/HBSocket
	../src/HBSocketControlService
	../src/HBSystem
	../src/HBThread
	../src/HBTime
	../src/Logging/Logger
	../src/Logging/LogSink
	../src/Logging/LogSinkFile
	../src/Logging/LogSinkConsole
	../src/Logging/LogSinkNet
)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS_INSTALL
    libgcc_s_dw2-1
    libstdc++-6
)
SET (LIBS_WINDOWS
	ws2_32
	Psapi
)
	
# USED LIBRARIES for BSD environment
SET (LIBS_BSD
	rt
)

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	rt
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
)

##############################################################
SET (TARGET_LIB_NAME
	HomerBase
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)