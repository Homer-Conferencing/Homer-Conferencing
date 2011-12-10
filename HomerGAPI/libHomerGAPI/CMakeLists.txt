###############################################################################
# Author:  Thomas Volkert
# Since:   2011-12-08
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

MESSAGE("#####################################")
MESSAGE("Configure HomerGAPI")
MESSAGE("#####################################")

##############################################################
# Configuration
##############################################################

# include dirs
SET (INCLUDE_DIRS
    ../include
	../../HomerBase/include
	../../HomerBase/include/Logging
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
    ../src/GAPI
    ../src/Requirements
    ../src/SocketName
    ../src/SocketSetup
    ../src/SocketSubscription
)

# USED LIBRARIES for win32 environment
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
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
    HomerBase
)

SET (TARGET_NAME
	HomerGAPI
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)