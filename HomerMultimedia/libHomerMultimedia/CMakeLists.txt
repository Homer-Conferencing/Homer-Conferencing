###############################################################################
# CMakeLists for libHomerMultimedia
# Author:  Thomas Volkert
# Since:   2011-02-19
# Version: $Id$
###############################################################################

MESSAGE("#####################################")
MESSAGE("Configure HomerMultimedia")
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
	../../HomerWinLibs/FFMPEG
	/usr/include/ffmpeg
	/usr/include/libavutil
	/usr/include/libavcodec
	/usr/include/libavformat
	/usr/include/libavdevice
	/usr/include/libswscale
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
	-Wno-uninitialized
	-static
)
endif (NOT APPLE)

# SOURCES
SET (SOURCES
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
)
ENDIF (APPLE)

IF (UNIX AND NOT WIN32 AND NOT APPLE)
SET (SOURCES
	${SOURCES}
	../src/MediaSourceAlsa
	../src/MediaSourceOss
	../src/MediaSourceV4L2
)
ENDIF (UNIX AND NOT WIN32 AND NOT APPLE)

# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS
	HomerBase
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

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
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