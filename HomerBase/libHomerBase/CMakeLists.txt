###############################################################################
# CMakeLists for libHomerBase
# Author:  Thomas Volkert
# Since:   2010-11-13
# Version: $Id$
###############################################################################

MESSAGE("#####################################")
MESSAGE("Configure HomerBase")
MESSAGE("#####################################")

##############################################################
# Configuration
##############################################################

# include dirs
SET (INCLUDE_DIRS
	../include
	../include/Logging
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
	-DWINVER=0x0501
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
	../src/HBMutex
	../src/HBCondition
	../src/HBRandom
	../src/HBReflection
	../src/HBSocket
	../src/HBSystem
	../src/HBThread
	../src/HBTime
	../src/Logging/Logger
	../src/Logging/LogSink
	../src/Logging/LogSinkFile
	../src/Logging/LogSinkConsole
	../src/Logging/LogSinkNet
)

# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	ws2_32
	Psapi
)
	
# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	rt
)

SET (TARGET_NAME
	HomerBase
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)