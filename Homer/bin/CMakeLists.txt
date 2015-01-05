###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-12
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeConfig.txt)

##############################################################
# Configuration
##############################################################

IF(WINDOWS)
	SET(HOMER_QT5 "true")
ENDIF()

##############################################################
# find package "Qt5" and include some macros
if(HOMER_QT5)
	MESSAGE("##### Searching for library Qt5")
	
	#set(CMAKE_AUTOMOC ON)
	set(CMAKE_INCLUDE_CURRENT_DIR ON)

	Find_Package(Qt5 COMPONENTS Core Gui LinguistTools Network WebKitWidgets Widgets Xml)
	
	include_directories(${Qt5Core_INCLUDE_DIRS})
	include_directories(${Qt5Gui_INCLUDE_DIRS})
	include_directories(${Qt5Network_INCLUDE_DIRS})
	include_directories(${Qt5WebKitWidgets_INCLUDE_DIRS})
	include_directories(${Qt5Widgets_INCLUDE_DIRS})
	include_directories(${Qt5Xml_INCLUDE_DIRS})
	
	add_definitions(${Qt5Widgets_DEFINITIONS})
ENDIF()

##############################################################
# find package "Qt4" and include some macros
if(NOT HOMER_QT5)
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
	SET(QT_USE_QTDBUS 1)
	SET(QT_USE_QTCOREL 1)
	SET(QT_USE_QTGUI 1)
	SET(QT_USE_QTNETWORK 1)
	SET(QT_USE_QTWEBKIT 1)
	SET(QT_USE_QTXML 1)
	INCLUDE(${QT_USE_FILE})
ENDIF()

##############################################################
# create config.h
IF (DEFINED INSIDE_HOMER_BUILD)
    INCLUDE (CheckIncludeFiles)
    CONFIGURE_FILE(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../${RELOCATION_INCLUDES}Homer/resources/BuildConfigHomer.h.in 
        ${CMAKE_CURRENT_BINARY_DIR}/BuildConfigHomer.h
    )
ENDIF()

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
    ${CMAKE_BINARY_DIR}/bin
    ${CMAKE_BINARY_DIR}/Homer/bin
    ${CMAKE_BINARY_DIR}/HomerMultimedia/libHomerMultimedia
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

IF (WINDOWS)
	SET(WIN32_EXECUTABLE ON)
	IF (${BUILD} MATCHES "Debug")
		SET (LFLAGS	"${LFLAGS} -Wl,--subsystem,console")
	ELSE()
		SET (LFLAGS	"${LFLAGS} -Wl,--subsystem,windows")
	ENDIF ()
ENDIF (WINDOWS)

##############################################################
# SOURCES 
SET (SOURCES
	../src/AudioPlayback
	../src/Configuration
	../src/ContactsManager
	../src/FileTransfersManager
	../src/HomerApplication
	../src/MainWindow
	../src/MediaFilterSystemState
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
	../include/ContactsManager.h
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
#    ../lang/Homer_ru.ts
#    ../lang/Homer_de.ts
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
if(HOMER_QT5)
	QT5_CREATE_TRANSLATION(QT_LANG_BINS ${QT_TRANS_SOURCES} ${QT_LANG_FILES} OPTIONS -no-obsolete)
ELSE()
	QT4_CREATE_TRANSLATION(QT_LANG_BINS ${QT_TRANS_SOURCES} ${QT_LANG_FILES} OPTIONS -no-obsolete)
ENDIF()

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
		icudt53.dll
		icuin53.dll
		icuuc53.dll
		libgcc_s_dw2-1.dll
		libstdc++-6.dll
		libwinpthread-1.dll
		mingwm10.dll
		Qt5Core.dll
		Qt5Gui.dll
		Qt5Multimedia.dll
		Qt5MultimediaWidgets.dll
		Qt5Network.dll
		Qt5OpenGL.dll
		Qt5Positioning.dll
		Qt5PrintSupport.dll
		Qt5Qml.dll
		Qt5Quick.dll
		Qt5Sensors.dll
		Qt5Sql.dll
		Qt5WebChannel.dll
		Qt5WebKit.dll
		Qt5WebKitWidgets.dll
		Qt5Widgets.dll
		Qt5Xml.dll
		Qt5XmlPatterns.dll
		phonon4.dll
		imageformats/qgif.dll
		imageformats/qico.dll
		imageformats/qjpeg.dll
		imageformats/qmng.dll
		imageformats/qsvg.dll
		imageformats/qtga.dll
		imageformats/qtiff.dll
		platforms/qwindows.dll
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
)
IF(HOMER_QT5)
	SET (LIBS_WINDOWS
			${LIBS_WINDOWS}
			${Qt5Core_LIBRARIES}
			${Qt5Gui_LIBRARIES}
			${Qt5Network_LIBRARIES}
			${Qt5WebKitWidgets_LIBRARIES}
			${Qt5Widgets_LIBRARIES}
			${Qt5Xml_LIBRARIES}
	)
ELSE()
	SET (LIBS_WINDOWS
			${LIBS_WINDOWS}
			${QT_LIBRARIES}
			${QT_QTCORE_LIBRARIES}
			${QT_QTGUI_LIBRARIES}
			${QT_QTNETWORK_LIBRARIES}
			${QT_QTXML_LIBRARIES}
			${QT_QTWEBKIT_LIBRARIES}
	)
ENDIF()

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
		QtDBus
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
		libstdc++.so.6.0.18
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
		plugins/imageformats/libqgif.so
		plugins/imageformats/libqico.so
		plugins/imageformats/libqjpeg.so
		plugins/imageformats/libqmng.so
		plugins/imageformats/libqsvg.so
		plugins/imageformats/libqtga.so
		plugins/imageformats/libqtiff.so
	)
	SET (LIBS_LINUX_STATIC
		#glib-2.0  # avoid link problems in QtWebKit
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
		libQtCore.dylib
		libQtGui.dylib
		libQtNetwork.dylib
		libQtXml.dylib
		libQtXmlPatterns.dylib
		libQtWebKit.dylib
		libQtCore.4.dylib
		libQtGui.4.dylib
		libQtNetwork.4.dylib
		libQtXml.4.dylib
		libQtXmlPatterns.4.dylib
		libQtWebKit.4.dylib
		libQtCore.4.8.4.dylib
		libQtGui.4.8.4.dylib
		libQtNetwork.4.8.4.dylib
		libQtXml.4.8.4.dylib
		libQtXmlPatterns.4.8.4.dylib
		libQtWebKit.4.9.3.dylib
		plugins/imageformats/libqgif.dylib
		plugins/imageformats/libqico.dylib
		plugins/imageformats/libqjpeg.dylib
		plugins/imageformats/libqmng.dylib
		plugins/imageformats/libqsvg.dylib
		plugins/imageformats/libqtga.dylib
		plugins/imageformats/libqtiff.dylib
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
	CoreServices
	ApplicationServices
)

##############################################################
SET (TARGET_PROGRAM_NAME
	Homer
)

##############################################################
# compile Qt-ui files via uic
if(HOMER_QT5)
	QT5_WRAP_UI(SOURCES  ${QT_UI_SOURCES})
ELSE()
	QT4_WRAP_UI(SOURCES ${QT_UI_SOURCES})
ENDIF()

##############################################################
# parse Q_OBJECT entries via moc
if(HOMER_QT5)
	QT5_WRAP_CPP(SOURCES ${QT_MOC_SOURCES})
ELSE()
	QT4_WRAP_CPP(SOURCES ${QT_MOC_SOURCES})
ENDIF()

##############################################################
# integrate Qt resources
if(HOMER_QT5)
	QT5_ADD_RESOURCES(SOURCES ${QT_QRC_SOURCES})
ELSE()
	QT4_ADD_RESOURCES(SOURCES ${QT_QRC_SOURCES})
ENDIF()

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

##############################################################
# check if "lang" directory exists
IF(NOT EXISTS "${TARGET_DIRECTORY}lang/")
	MESSAGE("##### Creating directory: ${TARGET_DIRECTORY}lang/")
	ADD_CUSTOM_COMMAND(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${TARGET_DIRECTORY}lang/)
ENDIF()

# copy translation binaries to destination folder
FOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
    ADD_CUSTOM_COMMAND(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${QT_LANG_BINS_ENTRY} ${TARGET_DIRECTORY}lang/)
ENDFOREACH(QT_LANG_BINS_ENTRY ${QT_LANG_BINS})
