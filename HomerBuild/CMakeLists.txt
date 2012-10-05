###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-28
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/CMakeConfig.txt)

###############################################################################
# we are inside HomerBuild!
SET (INSIDE_HOMER_BUILD "true")
cmake_minimum_required (VERSION 2.6)
PROJECT(Homer-Conferencing)
ADD_SUBDIRECTORY(../HomerBase ${CMAKE_CURRENT_BINARY_DIR}/HomerBase)
ADD_SUBDIRECTORY(../HomerMonitor ${CMAKE_CURRENT_BINARY_DIR}/HomerMonitor)
ADD_SUBDIRECTORY(../HomerNAPI ${CMAKE_CURRENT_BINARY_DIR}/HomerNAPI)
ADD_SUBDIRECTORY(../HomerMultimedia ${CMAKE_CURRENT_BINARY_DIR}/HomerMultimedia)
IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../HomerNetworkSimulator/HomerNetworkSimulator" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../HomerNetworkSimulator/HomerNetworkSimulator")
	MESSAGE("######## NetworkSimulator directory found")
	ADD_SUBDIRECTORY(../../HomerNetworkSimulator/HomerNetworkSimulator ${CMAKE_CURRENT_BINARY_DIR}/HomerNetworkSimulator)
ENDIF()
ADD_SUBDIRECTORY(../HomerConference ${CMAKE_CURRENT_BINARY_DIR}/HomerConference)
if (APPLE)
	ADD_SUBDIRECTORY(../HomerSoundOutput ${CMAKE_CURRENT_BINARY_DIR}/HomerSoundOutput)
endif()
ADD_SUBDIRECTORY(../Homer ${CMAKE_CURRENT_BINARY_DIR}/Homer)
IF (${BUILD} MATCHES "Release")
	ADD_SUBDIRECTORY(../../Homer-Release/HomerSounds ${CMAKE_CURRENT_BINARY_DIR}/HomerSounds)
ENDIF()

##############################################################################
# include CPack configuration to generate installers for Win/OSX
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/CMakeCPack.txt)
