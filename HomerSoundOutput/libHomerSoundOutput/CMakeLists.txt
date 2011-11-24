###############################################################################
# Author:  Thomas Volkert
# Since:   2011-02-19
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

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
if (LINUX)
SET (SOURCES
	${SOURCES}
	../src/WaveOutAlsa
)
endif (LINUX)

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
)

# USED STATIC libraries for linux environment ==> ordering of linking is IMPORTANT!!
SET (LIBS_APPLE_STATIC
	SDL_mixer
	smpeg
	SDL_sound
	SDL
	speex
	vorbisfile
	vorbis
	ogg
)

# USED FRAMEWORKS for apple environment
SET (FRAMEWORKS_APPLE
	AppKit
	AudioUnit
	CoreFoundation
	Carbon
	IOKit
	OpenGL
)

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
	HomerMonitor
	asound
)

# USED STATIC libraries for linux environment
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

# USED LIBRARIES for BSD environment
SET (LIBS_BSD
    HomerBase
    HomerMonitor
#    asound
)

# USED STATIC libraries for BSD environment
SET (LIBS_BSD_STATIC
    SDL_mixer
#    smpeg
    SDL_sound
    SDL
    vgl
    usbhid 
#    speex
#    FLAC
#    vorbisfile
#    vorbis
#    ogg
)
SET (TARGET_NAME
	HomerSoundOutput
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)