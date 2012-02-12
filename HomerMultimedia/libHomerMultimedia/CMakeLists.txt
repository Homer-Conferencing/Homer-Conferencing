###############################################################################
# Author:  Thomas Volkert
# Since:   2011-02-19
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

##############################################################
# Configuration
##############################################################

# include dirs
SET (INCLUDE_DIRS
	${INCLUDE_DIRS}
	../include
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerGAPI/include
	../../HomerMonitor/include
	../../HomerMultimedia/include
	../../HomerWinLibs/FFMPEG
	/usr/include/ffmpeg
)

# target directory for the lib
SET (TARGET_DIRECTORY
	${RELOCATION_DIR}../../lib
)

if (NOT APPLE)
# compile flags                                                                                                                                                                                                                                                                   
SET (FLAGS
	${FLAGS}
	-Wno-uninitialized
)
endif (NOT APPLE)

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
	../src/MediaSourceMMSys
	../src/MediaSourceVFW
)
ENDIF (WIN32)

IF (APPLE)
SET (SOURCES
	${SOURCES}
    ../src/MediaSourceCoreAudio
    ../src/MediaSourceCoreVideo
)
ENDIF (APPLE)

IF (BSD)
SET (SOURCES
	${SOURCES}
	../src/MediaSourceAlsa
	../src/MediaSourceOss
)
ENDIF (BSD)

IF (LINUX)
SET (SOURCES
	${SOURCES}
	../src/MediaSourceAlsa
	../src/MediaSourceOss
	../src/MediaSourceV4L2
)
ENDIF (LINUX)

# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	HomerBase
	HomerGAPI
	HomerMonitor
	ws2_32
	avutil-51
	avcodec-53
	avformat-53
	avdevice-53
	swscale-2
	Avicap32
	Winmm
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
	swscale
	avutil
	mp3lame
	opencore-amrnb
	opencore-amrwb
	x264
	bz2
	z
)

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
	swscale
	avutil
	mp3lame
	faac
	opencore-amrnb
	opencore-amrwb
	jack
	x264
	bz2
	z
)

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
)

SET (TARGET_NAME
	HomerMultimedia
)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)