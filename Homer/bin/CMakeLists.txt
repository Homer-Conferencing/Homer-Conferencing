###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-12
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# find package "QT" and include some macros
MESSAGE("##### Searching for library QT4")
Find_Package(Qt4 4.6 REQUIRED)
IF (NOT QT4_FOUND)
	MESSAGE(FATAL_ERROR "You need library QT4")
ENDIF ()
SET(QT_USE_QTCOREL 1)
SET(QT_USE_QTGUI 1)
SET(QT_USE_QTMULTIMEDIA 1)
SET(QT_USE_QTNETWORK 1)
SET(QT_USE_QTWEBKIT 1)
SET(QT_USE_QTXML 1)
INCLUDE(${QT_USE_FILE})

##############################################################
#

##############################################################
# include dirs
SET (INCLUDE_DIRS
	../include
	../forms
	../include/Dialogs
	../include/Widgets
	../../HomerBase/include/Logging
	../../HomerBase/include
	../../HomerGAPI/include
	../../HomerMonitor/include
	../../HomerMultimedia/include
	../../HomerConference/include
	../../HomerSoundOutput/include
	../../HomerWinLibs/QT/include/
	../../HomerWinLibs/QT/include/Qt
	../../HomerWinLibs/QT/include/QtCore
	../../HomerWinLibs/QT/include/QtGui
	../../HomerWinLibs/QT/include/QtMultimedia
	../../HomerWinLibs/QT/include/QtNetwork
	../../HomerWinLibs/QT/include/QtXml
	../../HomerWinLibs/QT/include/QtWebKit
	../../HomerWinLibs/FFMPEG
	../../HomerWinLibs/OPENSSL/include
	${QT_INCLUDE_DIR}
)

##############################################################
# target directory for the lib
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
# link flags
SET (LFLAGS
	${LFLAGS}
	-Wl,-R. 
	-Wl,-R./bin 
	-Wl,-R./lib 
	-Wl,-R../../lib 
	-Wl,-R../lib
	-Wl,-R../../bin/lib 
	-Wl,-R../bin/lib
	-mthreads
)

IF (WIN32)
	SET (LFLAGS
		${LFLAGS}
		../winres/Homer-WinRes.o	
	)
	IF (NOT (${BUILD} MATCHES "Debug"))
		SET (LFLAGS
			${LFLAGS}
			-Wl,-subsystem,windows
		)
	ENDIF ()
ENDIF (WIN32)

##############################################################
# SOURCES 
SET (SOURCES
	../src/Configuration
	../src/ContactsPool
	../src/MainWindow
	../src/MediaSourceDesktop
	../src/main
	../src/Dialogs/AddNetworkSinkDialog
	../src/Dialogs/ConfigurationDialog
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
	../src/Widgets/MessageHistory
	../src/Widgets/MessageInput
	../src/Widgets/MessageWidget
	../src/Widgets/OverviewContactsWidget
	../src/Widgets/OverviewDataStreamsWidget
	../src/Widgets/OverviewErrorsWidget
	../src/Widgets/OverviewFileTransfersWidget
	../src/Widgets/OverviewNetworkStreamsWidget
	../src/Widgets/OverviewPlaylistWidget
	../src/Widgets/OverviewThreadsWidget
	../src/Widgets/ParticipantWidget
	../src/Widgets/SessionInfoWidget
	../src/Widgets/StreamPositionWidget
	../src/Widgets/StreamingControlWidget
	../src/Widgets/VideoWidget
)
SET (QT_MOC_SOURCES
	../include/MainWindow.h
	../include/Dialogs/AddNetworkSinkDialog.h
	../include/Dialogs/ConfigurationDialog.h
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
	../include/Widgets/MessageHistory.h
	../include/Widgets/MessageInput.h
	../include/Widgets/MessageWidget.h
	../include/Widgets/OverviewContactsWidget.h
	../include/Widgets/OverviewDataStreamsWidget.h
	../include/Widgets/OverviewErrorsWidget.h
	../include/Widgets/OverviewFileTransfersWidget.h
	../include/Widgets/OverviewNetworkStreamsWidget.h
	../include/Widgets/OverviewPlaylistWidget.h
	../include/Widgets/OverviewThreadsWidget.h
	../include/Widgets/ParticipantWidget.h
	../include/Widgets/SessionInfoWidget.h
	../include/Widgets/StreamPositionWidget.h
	../include/Widgets/StreamingControlWidget.h
	../include/Widgets/VideoWidget.h
)
SET (QT_UI_SOURCES
	../forms/MainWindow.ui
	../forms/Dialogs/AddNetworkSinkDialog.ui
	../forms/Dialogs/ConfigurationDialog.ui
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
	../forms/Widgets/OverviewContactsWidget.ui
	../forms/Widgets/OverviewDataStreamsWidget.ui
	../forms/Widgets/OverviewErrorsWidget.ui
	../forms/Widgets/OverviewFileTransfersWidget.ui
	../forms/Widgets/OverviewNetworkStreamsWidget.ui
	../forms/Widgets/OverviewPlaylistWidget.ui
	../forms/Widgets/OverviewThreadsWidget.ui
	../forms/Widgets/ParticipantWidget.ui
	../forms/Widgets/SessionInfoWidget.ui
	../forms/Widgets/StreamPositionWidget.ui
	../forms/Widgets/StreamingControlWidget.ui
	../forms/Widgets/VideoWidget.ui
)

SET (QT_QRC_SOURCES
	../resources/Icons.qrc
)

##############################################################
# USED LIBRARIES for win32 environment
SET (LIBS_WINDOWS	
	stdc++
	HomerBase
	HomerGAPI
	HomerMonitor
	HomerMultimedia
	HomerConference
	HomerSoundOutput
	mingw32
	${QT_LIBRARIES}
	${QT_QTCORE_LIBRARIES}
	${QT_QTGUI_LIBRARIES}
	${QT_QTMULTIMEDIA_LIBRARIES}
	${QT_QTNETWORK_LIBRARIES}
	${QT_QTXML_LIBRARIES}
	${QT_QTWEBKIT_LIBRARIES}
#	QtCore4
#	QtGui4
#	QtMultimedia4
#	QtNetwork4
#	QtXml4
#	QtWebKit4
)
# USED LIBRARIES for BSD environment
SET (LIBS_BSD
)
# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	stdc++
	HomerBase
	HomerGAPI
	HomerMonitor
	HomerMultimedia
	HomerConference
	HomerSoundOutput
	${QT_LIBRARIES}
	dbus-1
	resolv
)
# USED LIBRARIES for apple environment
SET (LIBS_APPLE
)

##############################################################
SET (TARGET_PROGRAM_NAME
	Homer
)

##############################################################
# compile Qt-ui files via uic
QT4_WRAP_UI(UI_HEADERS ${QT_UI_SOURCES})

##############################################################
# parse Q_OBJECT entries via moc
QT4_WRAP_CPP(SOURCES ${QT_MOC_SOURCES})

##############################################################
# integrate Qt resources
QT4_ADD_RESOURCES(SOURCES ${QT_QRC_SOURCES})

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)