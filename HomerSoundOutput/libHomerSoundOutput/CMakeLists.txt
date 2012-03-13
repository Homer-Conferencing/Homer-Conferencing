###############################################################################
# Author:  Thomas Volkert
# Since:   2011-02-19
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# find package "ALSA"
IF (LINUX)
MESSAGE("##### Searching for library ALSA")
Find_Package(ALSA REQUIRED)
IF (NOT ALSA_FOUND)
    MESSAGE(FATAL_ERROR "You need library ALSA")
ENDIF ()
ELSE (LINUX)
    SET(ALSA_INCLUDE_DIRS "/usr/include")
ENDIF (LINUX)

##############################################################
# find package "SDL"
IF (LINUX)
MESSAGE("##### Searching for library SDL")
Find_Package(SDL REQUIRED)
IF (NOT SDL_FOUND)
	MESSAGE(FATAL_ERROR "You need library SDL")
ENDIF ()
ELSE (LINUX)
    SET(SDL_INCLUDE_DIR "/usr/include")
ENDIF (LINUX)

##############################################################
# find package "SDL_sound
IF (LINUX)
MESSAGE("##### Searching for library SDL_sound")
Find_Package(SDL_sound REQUIRED)
IF (NOT SDL_SOUND_FOUND)
	MESSAGE(FATAL_ERROR "You need library SDL_sound")
ENDIF ()
ELSE (LINUX)
    SET(SDL_SOUND_INCLUDE_DIR "/usr/include")
ENDIF (LINUX)

##############################################################
# find package "SDL_mixer
IF (LINUX)
MESSAGE("##### Searching for library SDL_mixer")
Find_Package(SDL_mixer REQUIRED)
IF (NOT SDLMIXER_FOUND)
	MESSAGE(FATAL_ERROR "You need library SDL_mixer")
ENDIF ()
ELSE (LINUX)
    SET(SDLMIXER_INCLUDE_DIR "/usr/include")
ENDIF (LINUX)


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
	${ALSA_INCLUDE_DIRS}
	${SDL_INCLUDE_DIR}
	${SDL_SOUND_INCLUDE_DIR}
	${SDLMIXER_INCLUDE_DIR}
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
if (NOT APPLE)
SET (FLAGS
	${FLAGS}
)
endif (NOT APPLE)

##############################################################
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

##############################################################
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
	SDL_sound
	SDL
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

IF (${BUILD} MATCHES "Default")
	SET (LIBS_LINUX
		HomerBase
		HomerMonitor
		${ALSA_LIBRARIES}
		${SDL_LIBRARY}
		${SDL_SOUND_LIBRARY}
		${SDLMIXER_LIBRARY}
	)
ELSE ()
	# USED LIBRARIES for linux environment
	SET (LIBS_LINUX
		HomerBase
		HomerMonitor
		asound
		resolv
	)

	# USED STATIC libraries for linux environment
	SET (LIBS_LINUX_STATIC
		SDL_mixer
		smpeg
		SDL_sound
		SDL
		pulse-simple
		pulse
		pulsecommon-0.9.22
		asyncns
		audio
		dbus-1
	)
ENDIF ()

# USED LIBRARIES for BSD environment
SET (LIBS_BSD
	HomerBase
	HomerMonitor
)

# USED STATIC libraries for BSD environment
SET (LIBS_BSD_STATIC
	SDL_mixer
	SDL_sound
	SDL
	vgl
	usbhid
)
##############################################################
SET (TARGET_LIB_NAME
	HomerSoundOutput
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)