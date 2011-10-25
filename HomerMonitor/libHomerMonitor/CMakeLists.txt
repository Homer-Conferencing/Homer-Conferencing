###############################################################################
# CMakeLists for libHomerMonitor
# Author:  Thomas Volkert
# Since:   2010-11-13
# Version: $Id$
###############################################################################

MESSAGE("#####################################")
MESSAGE("Configure HomerMonitor")
MESSAGE("#####################################")

##############################################################
# Configuration
##############################################################

# include directories
SET (INCLUDE_DIRS
	../include
	../../HomerBase/include
	../../HomerBase/include/Logging
)
	
# library directories
SET (LIB_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/../../lib
	${CMAKE_CURRENT_SOURCE_DIR}/../../HomerWinLibs
)
		
# target directory for the lib
SET (TARGET_DIRECTORY
	../../lib
)
	
# compile flags
SET (FLAGS

)


# SOURCES
SET (SOURCES
	../src/PacketStatistic
	../src/PacketStatisticService
	../src/ProcessStatistic
	../src/ProcessStatisticService
)

# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	HomerBase
)

# USED LIBRARIES for linux environment
set(LIBS_LINUX
	HomerBase
)

# USED LIBRARIES for apple environment
set(LIBS_APPLE
	HomerBase
)

SET (TARGET_NAME
	HomerMonitor
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)