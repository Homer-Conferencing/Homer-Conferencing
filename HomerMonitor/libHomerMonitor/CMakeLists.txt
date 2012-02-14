###############################################################################
# Author:  Thomas Volkert
# Since:   2010-11-13
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# include directories
SET (INCLUDE_DIRS
	../include
	../../HomerBase/include
	../../HomerBase/include/Logging
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

)

##############################################################
# SOURCES
SET (SOURCES
	../src/PacketStatistic
	../src/PacketStatisticService
	../src/ProcessStatistic
	../src/ProcessStatisticService
)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	HomerBase
)

# USED LIBRARIES for linux environment
set(LIBS_LINUX
	HomerBase
)

# USED LIBRARIES for BSD environment
set(LIBS_BSD
	HomerBase
)

# USED LIBRARIES for apple environment
set(LIBS_APPLE
	HomerBase
)

##############################################################
SET (TARGET_LIB_NAME
	HomerMonitor
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)