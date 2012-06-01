###############################################################################
# Author:  Thomas Volkert
# Since:   2012-05-31
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

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
	../../HomerMultimedia/include
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
	../src/Core/ChannelName
	../src/Core/ChannelSetup
	../src/Core/ChannelBinding
	../src/Core/ChannelConnection
	../src/Core/Cep
	../src/Core/Coordinator
	../src/Core/DomainNameService
	../src/Core/Link
	../src/Core/Node
	../src/Core/Scenario
	../src/NetworkSimulator
)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS_INSTALL
    libgcc_s_dw2-1
    libstdc++-6
)
SET (LIBS_WINDOWS
	HomerBase
	HomerGAPI
	HomerMultimedia
)
	
# USED LIBRARIES for BSD environment
SET (LIBS_BSD
	HomerBase
	HomerGAPI
	HomerMultimedia
)

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
	HomerGAPI
	HomerMultimedia
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerGAPI
	HomerMultimedia
)

##############################################################
SET (TARGET_LIB_NAME
	HomerNetworkSimulator
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)