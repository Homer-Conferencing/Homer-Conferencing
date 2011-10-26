###############################################################################
# CMakeLists for libHomerSoundOutput
# Author:  Thomas Volkert
# Since:   2011-02-19
# Version: $Id$
###############################################################################

MESSAGE("#####################################")
MESSAGE("Configure HomerSoundOutput")
MESSAGE("#####################################")

##############################################################
# Configuration
##############################################################

# include dirs
SET (INCLUDE_DIRS
	../include
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerMonitor/include
	../../HomerMultimedia/include
	../../HomerWinLibs/SDL/include
	../../HomerWinLibs/SDL-MIXER/include
	../../HomerWinLibs/SDL-SOUND
	/usr/include/SDL 
	/usr/local/include/SDL 
)

# library directories
SET (LIB_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/../../HomerLibs/win32
	${CMAKE_CURRENT_SOURCE_DIR}/../../lib
	/usr/local/lib
	/usr/lib
)

# target directory for the lib
SET (TARGET_DIRECTORY
	../../lib
)

if (NOT APPLE)
# compile flags
SET (FLAGS
	${FLAGS}
	-static
)
endif (NOT APPLE)

SET (SOURCES
	${SOURCES}
	../src/AudioOutSdl
	../src/WaveOut
)
# SOURCES
if (WIN32)
SET (SOURCES
    ${SOURCES}
	../src/WaveOutMMSys
)
endif (WIN32)
if (UNIX AND NOT WIN32 AND NOT APPLE)
SET (SOURCES
	${SOURCES}
	../src/WaveOutAlsa
)
endif (UNIX AND NOT WIN32 AND NOT APPLE)

# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	HomerBase
	HomerMonitor
	SDL
	SDL_sound
	SDL_mixer
	Winmm
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerMonitor
	SDL_mixer
	smpeg
	SDL_sound
	SDL
	speex
	FLAC
	vorbisfile
	vorbis
	ogg
)


# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
	HomerMonitor
	asound
)

# USED STATIC libraries for linux environment ==> ordering of linking is IMPORTANT!!
SET (LIBS_LINUX_STATIC
	SDL_mixer
	smpeg
	SDL_sound
	SDL
	speex
	FLAC
	vorbisfile
	vorbis
	ogg
)

SET (TARGET_NAME
	HomerSoundOutput
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)