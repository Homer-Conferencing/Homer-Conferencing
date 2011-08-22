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
	-W
	-Wall
	-Wno-switch
	-Wno-unused
	-Wno-long-long
	-Wno-deprecated-declarations
	-O2
)

# "-D" options for compilation in win32 environment
SET (DEFINITIONS_WINDOWS
	-D__STDC_CONSTANT_MACROS
	-DWIN32
	-D_REENTRANT
)

# "-D" options for compilation in linux environment
SET (DEFINITIONS_LINUX
	-D__STDC_CONSTANT_MACROS
	-DLINUX
	-D_REENTRANT
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

SET (TARGET_NAME
	HomerMonitor
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)