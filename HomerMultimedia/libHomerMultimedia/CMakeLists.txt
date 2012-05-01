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
# include dirs
SET (INCLUDE_DIRS
	${INCLUDE_DIRS}
	../include
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerGAPI/include
	../../HomerMonitor/include
	/usr/include/ffmpeg
	${ALSA_INCLUDE_DIRS}
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
	../src/RTP
)

IF (WIN32)
SET (SOURCES
	${SOURCES}
	# included in port audio: ../src/MediaSourceMMSys
	../src/MediaSourcePortAudio
	../src/MediaSourceVFW
)
ENDIF (WIN32)

IF (APPLE)
SET (SOURCES
	${SOURCES}
    	# included in port audio: ../src/MediaSourceCoreAudio
    	../src/MediaSourceCoreVideo
		../src/MediaSourcePortAudio
)
ENDIF (APPLE)

IF (BSD)
SET (SOURCES
	${SOURCES}
	# included in port audio: ../src/MediaSourceAlsa
	# included in port audio: ../src/MediaSourceOss
	../src/MediaSourcePortAudio
)
ENDIF (BSD)

IF (LINUX)
SET (SOURCES
	${SOURCES}
	# included in port audio: ../src/MediaSourceAlsa
	# included in port audio: ../src/MediaSourceOss
	../src/MediaSourcePortAudio
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
# replaced by portaudio:    Winmm
	ws2_32
	Avicap32
)

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerGAPI
	HomerMonitor
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
)
SET (FRAMEWORKS_APPLE
	CoreFoundation
	CoreVideo
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
)

##############################################################
SET (TARGET_LIB_NAME
	HomerMultimedia
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)
