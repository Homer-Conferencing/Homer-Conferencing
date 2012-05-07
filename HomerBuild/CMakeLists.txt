###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-28
###############################################################################

# we are inside HomerBuild!
SET (INSIDE_HOMER_BUILD "true")
cmake_minimum_required (VERSION 2.6)
PROJECT(Homer-Conferencing)
ADD_SUBDIRECTORY(../HomerBase ${CMAKE_CURRENT_BINARY_DIR}/HomerBase)
ADD_SUBDIRECTORY(../HomerMonitor ${CMAKE_CURRENT_BINARY_DIR}/HomerMonitor)
ADD_SUBDIRECTORY(../HomerGAPI ${CMAKE_CURRENT_BINARY_DIR}/HomerGAPI)
ADD_SUBDIRECTORY(../HomerMultimedia ${CMAKE_CURRENT_BINARY_DIR}/HomerMultimedia)
ADD_SUBDIRECTORY(../HomerConference ${CMAKE_CURRENT_BINARY_DIR}/HomerConference)
ADD_SUBDIRECTORY(../Homer ${CMAKE_CURRENT_BINARY_DIR}/Homer)
IF (${BUILD} MATCHES "Release")
	ADD_SUBDIRECTORY(../../Homer-Release/HomerSounds ${CMAKE_CURRENT_BINARY_DIR}/HomerSounds)
ENDIF()

##############################################################################
# include CPack configuration to generate installers for Win/OSX
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../HomerBuild/CMakeCPack.txt)
