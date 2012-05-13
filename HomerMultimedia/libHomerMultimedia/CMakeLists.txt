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
# include dirs
SET (INCLUDE_DIRS
	${INCLUDE_DIRS}
	../include
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerGAPI/include
	../../HomerMonitor/include
	../../HomerSoundOutput/include
	/usr/include/ffmpeg
	${ALSA_INCLUDE_DIRS}
	${SDL_INCLUDE_DIR}	
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
IF (NOT APPLE)
SET (FLAGS
	${FLAGS}
	-Wno-uninitialized
)
ENDIF (NOT APPLE)

##############################################################
# SOURCES
SET (SOURCES
	../src/MediaFifo
	../src/MediaSink
	../src/MediaSinkFile
	../src/MediaSinkNet
	../src/MediaSource
	../src/MediaSourceFile
	../src/MediaSourceMem
	../src/MediaSourceMuxer
	../src/MediaSourceNet
	../src/MediaSourcePortAudio
	../src/RTP
	../src/WaveOut
	../src/WaveOutPortAudio	
)

IF (WIN32)
SET (SOURCES
	${SOURCES}
	#TODO: ../src/MediaSourceDShow
	../src/MediaSourceVFW
)
ENDIF (WIN32)

IF (APPLE)
SET (SOURCES
	${SOURCES}
	../src/MediaSourceCoreVideo
	../src/WaveOutSdl
)
ENDIF (APPLE)

IF (BSD)
SET (SOURCES
	${SOURCES}
)
ENDIF (BSD)

IF (LINUX)
SET (SOURCES
	${SOURCES}
	../src/MediaSourceV4L2
)
ENDIF (LINUX)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS_INSTALL
    avutil-51
    avcodec-53
    avdevice-53
    avfilter-2
    avformat-53
    libgcc_s_dw2-1
    libstdc++-6
    portaudio
    swresample-0
    swscale-2
    SDL    
)
SET (LIBS_WINDOWS
    avutil-51
    avcodec-53
    avformat-53
    avdevice-53
	HomerBase
	HomerGAPI
	HomerMonitor
	portaudio
    swscale-2
	ws2_32
	Avicap32
	SDL
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerGAPI
	HomerMonitor
	HomerSoundOutput
)
SET (LIBS_APPLE_STATIC
	avdevice
	avformat
	avcodec
	avfilter
	swresample
	swscale
	avutil
	mp3lame
	opencore-amrnb
	opencore-amrwb
	x264
	bz2
	z
	portaudio
	SDL
)
SET (FRAMEWORKS_APPLE
	AppKit
	AudioToolbox
	AudioUnit
	Carbon
	CoreAudio
	CoreFoundation
	CoreVideo
	IOKit
	OpenGL	
	VideoDecodeAcceleration
)

# used libraries for LINUX environment
IF (${BUILD} MATCHES "Default")
	SET (LIBS_LINUX
		HomerBase
		HomerGAPI
		HomerMonitor
		${ALSA_LIBRARIES}
		avdevice
		avformat
		avcodec
		avutil
		avfilter
		swscale
		x264
		${SDL_LIBRARY}		
		portaudio
	)
ELSE ()
	# USED LIBRARIES for linux environment
	SET (LIBS_LINUX
		HomerBase
		HomerGAPI
		HomerMonitor
		asound
	)

	SET (LIBS_LINUX_STATIC
		avdevice
		avformat
		avcodec
		avfilter
		swresample
		swscale
		avutil
		mp3lame
		faac
		opencore-amrnb
		opencore-amrwb
		jack
		theoraenc
		theoradec
		ogg
		x264
		bz2
		z		
		SDL
		portaudio
	)
ENDIF ()

# USED LIBRARIES for BSD environment
SET (LIBS_BSD
	HomerBase
	HomerGAPI
	HomerMonitor
)

SET (LIBS_BSD_STATIC
	avdevice
	avformat
	avcodec
	avfilter
	swscale
	avutil
	mp3lame
	opencore-amrnb
	opencore-amrwb
	jack
	x264
	bz2
	z
	portaudio
	SDL
)

##############################################################
SET (TARGET_LIB_NAME
	HomerMultimedia
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)
