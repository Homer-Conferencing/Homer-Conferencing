###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-12
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeConfig.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# find package "Qt4" and include some macros
MESSAGE("##### Searching for library Qt4")
SET (QT_QMAKE_EXECUTABLE $ENV{QT_QMAKE_EXECUTABLE} )
Find_Package(Qt4 4.7 REQUIRED)
IF (NOT QT4_FOUND)
	IF ($ENV{QT_QMAKE_EXECUTABLE} MATCHES "")
		MESSAGE(FATAL_ERROR "Library Qt4 not found, try to define the absolute path to Qt4 qmake executable in the environment variable QT_QMAKE_EXECUTABLE")
	ELSE()
		MESSAGE(FATAL_ERROR "You need library Qt4")
	ENDIF()
ELSE()
	MESSAGE("Qt4 binary path is: ${QT_BINARY_DIR}")
	MESSAGE("Qt4 moc path is: ${QT_MOC_EXECUTABLE}")
	MESSAGE("Qt4 uic path is: ${QT_UIC_EXECUTABLE}")
ENDIF ()
SET(QT_USE_QTCOREL 1)
SET(QT_USE_QTGUI 1)
SET(QT_USE_QTNETWORK 1)
SET(QT_USE_QTWEBKIT 1)
SET(QT_USE_QTXML 1)
INCLUDE(${QT_USE_FILE})

##############################################################
# include dirs
SET (INCLUDE_DIRS
	${INCLUDE_DIRS}
	../include
	../include/Dialogs
	../include/Widgets
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerNAPI/include
	../../HomerMonitor/include
	../../HomerMultimedia/include
	../../HomerNetworkSimulator/include
	../../HomerConference/include
	../../../HomerNetworkSimulator/HomerNetworkSimulator/include/
	/usr/include/ffmpeg
	${QT_INCLUDE_DIR}
	${CMAKE_BINARY_DIR}/Homer/bin
	${CMAKE_BINARY_DIR}/bin
	${CMAKE_BINARY_DIR}/HomerNetworkSimulator/libHomerNetworkSimulator
)

##############################################################
# target directory for the executable
SET (TARGET_DIRECTORY
	${RELOCATION_DIR}
)

##############################################################
# compile flags
SET (FLAGS
	${FLAGS}
	-D_GLIBCXX_USE_WSTRING
)

##############################################################
# set rapth entries for non-default builds
IF (NOT (${BUILD} MATCHES "Default"))
	IF (APPLE)
		SET (LFLAGS
			${LFLAGS}
			-Wl,-rpath,.
			-Wl,-rpath,./lib
			-Wl,-rpath,../lib
			-Wl,-rpath,../../lib
			-Wl,-rpath,./bin
			-Wl,-rpath,./bin/lib
			-Wl,-rpath,../bin/lib
			-Wl,-rpath,../../bin/lib
			-Wl,-rpath,/usr/lib
			-Wl,-rpath,/usr/local/lib
		)
		IF (DEFINED INSTALL_LIBDIR)
			SET (LFLAGS
				${LFLAGS}
				-Wl,-rpath,${INSTALL_LIBDIR}
			)
		ENDIF()
	ELSE(APPLE)
		SET (LFLAGS
			${LFLAGS}
			-Wl,-R.
			-Wl,-R./lib
			-Wl,-R../lib
			-Wl,-R../../lib
			-Wl,-R./bin
			-Wl,-R./bin/lib
			-Wl,-R../bin/lib
			-Wl,-R../../bin/lib
			-Wl,-R/usr/lib
			-Wl,-R/usr/local/lib
		)
		IF (DEFINED INSTALL_LIBDIR)
			SET (LFLAGS
				${LFLAGS}
				-Wl,-R${INSTALL_LIBDIR}
			)
		ENDIF()
	ENDIF()
ENDIF()

IF (WIN32)
	IF (${BUILD} MATCHES "Debug")
		SET (LFLAGS	"${LFLAGS} -Wl,--subsystem,console")
	ELSE()
		SET (LFLAGS	"${LFLAGS} -Wl,--subsystem,windows")
	ENDIF ()
ENDIF (WIN32)

##############################################################
# SOURCES 
SET (SOURCES
	../src/AudioPlayback
	../src/Configuration
	../src/ContactsManager
	../src/FileTransfersManager
	../src/HomerApplication
	../src/MainWindow
	../src/MediaSourceDesktop
	../src/MediaSourceLogo
	../src/MediaSourceGrabberThread
	../src/main
	../src/Dialogs/AddNetworkSinkDialog
	../src/Dialogs/ConfigurationDialog
	../src/Dialogs/ConfigurationAudioSilenceDialog
	../src/Dialogs/ContactEditDialog
	../src/Dialogs/FileTransferAckDialog
	../src/Dialogs/HelpDialog
	../src/Dialogs/IdentityDialog
	../src/Dialogs/OpenVideoAudioPreviewDialog
	../src/Dialogs/SegmentSelectionDialog
	../src/Dialogs/UpdateCheckDialog
	../src/Dialogs/VersionDialog
	../src/Widgets/AudioWidget
	../src/Widgets/AvailabilityWidget
	../src/Widgets/CallButton
	../src/Widgets/LinkedPixmapLabel
	../src/Widgets/MessageHistory
	../src/Widgets/MessageInput
	../src/Widgets/MessageWidget
	../src/Widgets/MovieControlWidget
	../src/Widgets/OverviewContactsWidget
	../src/Widgets/OverviewDataStreamsWidget
	../src/Widgets/OverviewErrorsWidget
	../src/Widgets/OverviewFileTransfersWidget
	../src/Widgets/OverviewNetworkStreamsWidget
	../src/Widgets/OverviewPlaylistWidget
	../src/Widgets/OverviewThreadsWidget
	../src/Widgets/ParticipantWidget
	../src/Widgets/PlaybackSlider
	../src/Widgets/SessionInfoWidget
	../src/Widgets/StreamingControlWidget
	../src/Widgets/VideoWidget
)
SET (QT_MOC_SOURCES
	../include/MainWindow.h
	../include/MediaSourceGrabberThread.h
	../include/Dialogs/AddNetworkSinkDialog.h
	../include/Dialogs/ConfigurationDialog.h
	../include/Dialogs/ConfigurationAudioSilenceDialog.h
	../include/Dialogs/ContactEditDialog.h
	../include/Dialogs/FileTransferAckDialog.h
	../include/Dialogs/HelpDialog.h
	../include/Dialogs/IdentityDialog.h
	../include/Dialogs/OpenVideoAudioPreviewDialog.h
	../include/Dialogs/SegmentSelectionDialog.h
	../include/Dialogs/UpdateCheckDialog.h
	../include/Dialogs/VersionDialog.h
	../include/Widgets/AudioWidget.h
	../include/Widgets/AvailabilityWidget.h
	../include/Widgets/CallButton.h
	../include/Widgets/LinkedPixmapLabel.h
	../include/Widgets/MessageHistory.h
	../include/Widgets/MessageInput.h
	../include/Widgets/MessageWidget.h
	../include/Widgets/MovieControlWidget.h
	../include/Widgets/OverviewContactsWidget.h
	../include/Widgets/OverviewDataStreamsWidget.h
	../include/Widgets/OverviewErrorsWidget.h
	../include/Widgets/OverviewFileTransfersWidget.h
	../include/Widgets/OverviewNetworkStreamsWidget.h
	../include/Widgets/OverviewPlaylistWidget.h
	../include/Widgets/OverviewThreadsWidget.h
	../include/Widgets/ParticipantWidget.h
	../include/Widgets/PlaybackSlider.h
	../include/Widgets/SessionInfoWidget.h
	../include/Widgets/StreamingControlWidget.h
	../include/Widgets/VideoWidget.h
)
SET (QT_UI_SOURCES
	../forms/MainWindow.ui
	../forms/Dialogs/AddNetworkSinkDialog.ui
	../forms/Dialogs/ConfigurationDialog.ui
	../forms/Dialogs/ConfigurationAudioSilenceDialog.ui
	../forms/Dialogs/ContactEditDialog.ui
	../forms/Dialogs/FileTransferAckDialog.ui
	../forms/Dialogs/HelpDialog.ui
	../forms/Dialogs/IdentityDialog.ui
	../forms/Dialogs/OpenVideoAudioPreviewDialog.ui
	../forms/Dialogs/SegmentSelectionDialog.ui
	../forms/Dialogs/UpdateCheckDialog.ui
	../forms/Dialogs/VersionDialog.ui
	../forms/Widgets/AudioWidget.ui
	../forms/Widgets/AvailabilityWidget.ui
	../forms/Widgets/MessageWidget.ui
	../forms/Widgets/MovieControlWidget.ui
	../forms/Widgets/OverviewContactsWidget.ui
	../forms/Widgets/OverviewDataStreamsWidget.ui
	../forms/Widgets/OverviewErrorsWidget.ui
	../forms/Widgets/OverviewFileTransfersWidget.ui
	../forms/Widgets/OverviewNetworkStreamsWidget.ui
	../forms/Widgets/OverviewPlaylistWidget.ui
	../forms/Widgets/OverviewThreadsWidget.ui
	../forms/Widgets/ParticipantWidget.ui
	../forms/Widgets/SessionInfoWidget.ui
	../forms/Widgets/StreamingControlWidget.ui
)

SET (QT_QRC_SOURCES
	../resources/Icons.qrc
)

##############################################################
# multi-language support
SET(QT_LANG_FILES
    ../lang/Homer_ru.ts
    ../lang/Homer_de.ts
)

file (GLOB HEADER_FILES 
		../include/*.h
		../include/Dialogs/*.h
		../include/Widgets/*.h
)

file (GLOB CPP_FILES 
		../src/*.cpp
		../src/Dialogs/*.cpp
		../src/Widgets/*.cpp
)

file (GLOB UI_FILES 
		../forms/*.ui
		../forms/Dialogs/*.ui
		../forms/Widgets/*.ui
)

SET(QT_TRANS_SOURCES
		${CPP_FILES} 
		${UI_FILES} 
		${HEADER_FILES}
)

# integrate Qt translations
QT4_CREATE_TRANSLATION(QT_LANG_BINS ${QT_TRANS_SOURCES} ${QT_LANG_FILES} OPTIONS -no-obsolete)

# copy translation binaries to destination folder
FOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
	FILE(COPY ${QT_LANG_BINS_ENTRY} DESTINATION ${TARGET_DIRECTORY}lang/)
ENDFOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})

# mark translator binaries for installation process
IF ((LINUX) AND (DEFINED INSTALL_DATADIR))
	FOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
		INSTALL(FILES ${QT_LANG_BINS_ENTRY} DESTINATION ${INSTALL_DATADIR}/lang COMPONENT lang)
	ENDFOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
ELSE()
	FOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
		INSTALL(FILES ${QT_LANG_BINS_ENTRY} DESTINATION lang COMPONENT lang)
	ENDFOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
ENDIF()

# add translator binaries to the linked sources
SET(SOURCES 
		${SOURCES}
		${QT_LANG_BINS}
)

##############################################################
#IF(EXISTS "../../HomerNetworkSimulator" AND IS_DIRECTORY "../../HomerNetworkSimulator")
IF (HOMER_NETWORK_SIMULATOR)
	SET(NETWORK_SIMULATOR "HomerNetworkSimulator")
ENDIF()

##############################################################
# USED LIBRARIES for win32 environment
IF (NOT (${BUILD} MATCHES "Default"))
	SET (LIBS_WINDOWS_INSTALL   
		libgcc_s_dw2-1
		libstdc++-6
		mingwm10
		QtCore4
		QtGui4
		QtNetwork4
		QtWebKit4
		QtXml4
		QtXmlPatterns4
		phonon4
		plugins/imageformats/qgif4
		plugins/imageformats/qico4
		plugins/imageformats/qjpeg4
		plugins/imageformats/qmng4
		plugins/imageformats/qsvg4
		plugins/imageformats/qtga4
		plugins/imageformats/qtiff4
	)
ENDIF()
SET (LIBS_WINDOWS	
	stdc++
	HomerBase
	HomerNAPI
	HomerMonitor
	HomerMultimedia
	${NETWORK_SIMULATOR}
	HomerConference
	mingw32
	${QT_LIBRARIES}
	${QT_QTCORE_LIBRARIES}
	${QT_QTGUI_LIBRARIES}
	${QT_QTNETWORK_LIBRARIES}
	${QT_QTXML_LIBRARIES}
	${QT_QTWEBKIT_LIBRARIES}
)

##############################################################
# USED LIBRARIES for BSD environment
SET (LIBS_BSD
        HomerBase
        HomerNAPI
        HomerMonitor
        HomerMultimedia
		${NETWORK_SIMULATOR}
        HomerConference
        ${QT_LIBRARIES}
)

##############################################################
# USED LIBRARIES for LINUX environment
SET (LIBS_LINUX
	HomerBase
	HomerNAPI
	HomerMonitor
	HomerMultimedia
	${NETWORK_SIMULATOR}
	HomerConference
)
IF (${BUILD} MATCHES "Default")
	SET (LIBS_LINUX
		${LIBS_LINUX}
		${QT_LIBRARIES}
	)
ELSE ()
	SET (LIBS_LINUX
		stdc++
		${LIBS_LINUX}
		QtCore
		QtGui
		QtNetwork
		QtXml
		QtWebKit
		resolv
	)
ENDIF ()
IF (NOT (${BUILD} MATCHES "Default"))
	SET (LIBS_LINUX_INSTALL
		libQtCore.so.4
		libQtGui.so.4
		libQtWebKit.so.4
		libgstapp-0.10.so
		libgstbase-0.10.so.0
		libgstinterfaces-0.10.so.0.24.0
		libgstreamer-0.10.so         
		libgstvideo-0.10.so.0
		libstdc++.so.6.0.14
		libQtCore.so.4.8.4
		libQtGui.so.4.8.4
		libQtWebKit.so.4.9.3
		libgstapp-0.10.so.0
		libgstbase-0.10.so.0.29.0
		libgstpbutils-0.10.so
		libgstreamer-0.10.so.0
		libgstvideo-0.10.so.0.24.0
		libQtDBus.so.4 
		libQtNetwork.so.4
		libQtXml.so.4  
		libQtXmlPatterns.so.4  
		libgstapp-0.10.so.0.24.0  
		libgstinterfaces-0.10.so    
		libgstpbutils-0.10.so.0          
		libgstreamer-0.10.so.0.29.0  
		libstdc++.so
		libgcc_s.so.1
		libQtDBus.so.4.8.4
		libQtNetwork.so.4.8.4
		libQtXml.so.4.8.4     
		libQtXmlPatterns.so.4.8.4     
		libgstbase-0.10.so
		libgstinterfaces-0.10.so.0  
		libgstpbutils-0.10.so.0.24.0     
		libgstvideo-0.10.so          
		libstdc++.so.6
		plugins/imageformats/qgif4
		plugins/imageformats/qico4
		plugins/imageformats/qjpeg4
		plugins/imageformats/qmng4
		plugins/imageformats/qsvg4
		plugins/imageformats/qtga4
		plugins/imageformats/qtiff4
	)
	SET (LIBS_LINUX_STATIC
		glib-2.0  # avoid link problems in QtWebKit
	)
ENDIF()

##############################################################
# USED LIBRARIES for APPLE environment
SET (LIBS_APPLE
	stdc++
	HomerBase
	HomerNAPI
	HomerMonitor
	HomerMultimedia
	${NETWORK_SIMULATOR}
	HomerConference
)
IF (NOT(${BUILD} MATCHES "Default"))
	SET (LIBS_APPLE_INSTALL
		QtCore
		QtGui
		QtNetwork
		QtXml
		QtXmlPatterns
		QtWebKit
		QtCore.4
		QtGui.4
		QtNetwork.4
		QtXml.4
		QtXmlPatterns.4
		QtWebKit.4
		QtCore.4.8.4
		QtGui.4.8.4
		QtNetwork.4.8.4
		QtXml.4.8.4
		QtXmlPatterns.4.8.4
		QtWebKit.4.9.3
		plugins/imageformats/qgif4
		plugins/imageformats/qico4
		plugins/imageformats/qjpeg4
		plugins/imageformats/qmng4
		plugins/imageformats/qsvg4
		plugins/imageformats/qtga4
		plugins/imageformats/qtiff4
	)
	SET (LIBS_APPLE
		${LIBS_APPLE}
		QtCore
		QtGui
		QtNetwork
		QtXml
		QtXmlPatterns
		QtWebKit
	)
else()
	SET (LIBS_APPLE
		${LIBS_APPLE}
		${QT_LIBRARIES}
	)
endif()

SET (FRAMEWORKS_APPLE
	Cocoa
	ApplicationServices
)

##############################################################
SET (TARGET_PROGRAM_NAME
	Homer
)

##############################################################
# compile Qt-ui files via uic
QT4_WRAP_UI(SOURCES ${QT_UI_SOURCES})

##############################################################
# parse Q_OBJECT entries via moc
QT4_WRAP_CPP(SOURCES ${QT_MOC_SOURCES})

##############################################################
# integrate Qt resources
QT4_ADD_RESOURCES(SOURCES ${QT_QRC_SOURCES})

##############################################################
# extend install target by "Homer.png" for non Windows systems
IF (LINUX) 
	IF (DEFINED INSTALL_DATADIR)
        	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../Homer.png DESTINATION ${INSTALL_DATADIR})
	ELSE()
        	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../Homer.png DESTINATION .)
	ENDIF()
ENDIF()

##############################################################
# Qt needs qt_menu.nib/keyedobjects.nib in the binary directory
# file from Qt-Source 4.8.2
if (APPLE)
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../../Homer-Release/HomerLibs/apple${ARCH_BITS}/qt_menu.nib/keyedobjects.nib DESTINATION lib/qt_menu.nib COMPONENT libraries)
endif()

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)
