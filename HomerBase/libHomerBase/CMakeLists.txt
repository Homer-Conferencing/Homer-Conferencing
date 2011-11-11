###############################################################################
# Author:  Thomas Volkert
# Since:   2010-11-13
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

# target directory for the lib
SET (TARGET_DIRECTORY
	../../lib
)
	
# compile flags
SET (FLAGS
	${FLAGS}
	
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

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
)

SET (TARGET_NAME
	HomerBase
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)