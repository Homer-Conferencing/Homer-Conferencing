###############################################################################
# Author:  Thomas Volkert
# Since:   2011-12-08
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
	../src/Events
	../src/NAPI
	../src/Requirements
	../src/Berkeley/SocketName
	../src/Berkeley/SocketSetup
	../src/Berkeley/SocketBinding
	../src/Berkeley/SocketConnection
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
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
)

##############################################################
SET (TARGET_LIB_NAME
	HomerNAPI
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)
