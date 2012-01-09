###############################################################################
# Makefile for 'Homer'
# Author:  Thomas Volkert
# Since:   2008-11-25
###############################################################################

TOP_DIR=	../..
include $(TOP_DIR)/HomerMake/config.mk

###############################################################################

# to have extended stdint.h definitions, e.g. INT64_C
CFLAGS+=	-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D_GLIBCXX_USE_WSTRING

CFLAGS+=	-D_OS_ARCH=\"$(OS_ARCH)\"
CFLAGS+=	-Wno-uninitialized

# for mingw32
ifeq ($(MSYSTEM),MINGW32)
CFLAGS+=	-mthreads 
endif
###############################################################################

IFLAGS+=	-I$(MULTIMEDIA_INCL_DIR)
IFLAGS+=	-I$(TOP_DIR)/HomerBase/include/Logging
IFLAGS+=	-I$(TOP_DIR)/HomerBase/include
IFLAGS+=	-I$(TOP_DIR)/HomerGAPI/include
IFLAGS+=	-I$(TOP_DIR)/HomerMonitor/include
IFLAGS+=	-I$(TOP_DIR)/HomerSoundOutput/include
IFLAGS+=	-I$(TOP_DIR)/HomerConference/include

IFLAGS+=	-I../include
IFLAGS+=	-I../forms

# for mingw32
ifeq ($(MSYSTEM),MINGW32)
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/Qt
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/QtCore
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/QtGui
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/QtMultimedia
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/QtNetwork
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/QtXml
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/QT/include/QtWebKit
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/FFMPEG
IFLAGS+=	-I$(TOP_DIR)/HomerWinLibs/OPENSSL/include
else
IFLAGS+=	-I$(QT4_INCL_DIR)
IFLAGS+= 	-I$(QT4_INCL_DIR)/QtMultimedia
IFLAGS+= 	-I$(QT4_INCL_DIR)/QtNetwork
IFLAGS+= 	-I$(QT4_INCL_DIR)/QtXml
IFLAGS+= 	-I$(QT4_INCL_DIR)/QtWebKit
IFLAGS+=	-I$(QT4_CORE_INCL_DIR)
IFLAGS+=	-I$(QT4_GUI_INCL_DIR)
endif

###############################################################################

LIB_DIRS+=	$(TOP_DIR)/HomerLibs/$(OS_ARCH)
LIB_DIRS+=	$(TOP_DIR)/lib
LIB_DIRS+=	/usr/local/lib
LIBS+=		stdc++
LIBS+=		HomerBase
LIBS+=		HomerGAPI
LIBS+=		HomerMonitor
LIBS+=		HomerMultimedia
LIBS+=		HomerSoundOutput
LIBS+=		HomerConference

ifeq ($(MSYSTEM),MINGW32)
LIBS+=		mingw32
LIBS+=		QtCore4
LIBS+=		QtGui4
LIBS+=		QtMultimedia4
LIBS+=		QtNetwork4
LIBS+=		QtXml4
LIBS+=		QtWebKit4
else
LIBS_STATIC+=	$(QT4_CORE_LIB)
LIBS_STATIC+=	$(QT4_GUI_LIB)
LIBS_STATIC+=	QtMultimedia
LIBS_STATIC+=	QtNetwork
LIBS_STATIC+=	QtXml
LIBS_STATIC+=	QtWebKit
LIBS+=		$(LIBS_STATIC)
LIBS+=		dbus-1
LIBS+=		resolv
#LFLAGS+=	-Wl,-Bstatic -Wl,-Bsymbolic -lQtWebKit -l$(QT4_GUI_LIB) -lQtMultimedia -lQtNetwork -lQtXml -lQtDBus -lQtXml -lQtScript -l$(QT4_CORE_LIB) -Wl,-Bdynamic
endif
LFLAGS+=	-Wl,-R. -Wl,-R./bin -Wl,-R./lib -Wl,-R../../lib -Wl,-R../lib

ifeq ($(MSYSTEM),MINGW32)
# otherwise the console window will be displayed
#LFLAGS+=	-Wl,-subsystem,windows
LFLAGS+=	-mthreads
LFLAGS+=	$(TOP_DIR)/Homer/ico/Homer-Icon.o
else
#CXXFLAGS+=	-static-libgcc -static-libstdc++
#LFLAGS+=	-static-libgcc -static-libstdc++
endif

###############################################################################

APP_NAME=	Homer

SOURCE=         main.cpp \
		AddNetworkSinkDialog.cpp \
		AudioWidget.cpp \
		AvailabilityWidget.cpp \
		CallButton.cpp \
		Configuration.cpp \
		ConfigurationDialog.cpp \
		ContactEditDialog.cpp \
		ContactsPool.cpp \
		FileTransferAckDialog.cpp \
		HelpDialog.cpp \
		IdentityDialog.cpp \
		MainWindow.cpp \
		MediaSourceDesktop.cpp \
		MessageHistory.cpp \
		MessageInput.cpp \
		MessageWidget.cpp \
		OpenVideoAudioPreviewDialog.cpp \
		OverviewContactsWidget.cpp \
		OverviewDataStreamsWidget.cpp \
		OverviewErrorsWidget.cpp \
		OverviewFileTransfersWidget.cpp \
		OverviewNetworkStreamsWidget.cpp \
		OverviewPlaylistWidget.cpp \
		OverviewThreadsWidget.cpp \
		ParticipantWidget.cpp \
		SegmentSelectionDialog.cpp \
		StreamingControlWidget.cpp \
		StreamPositionWidget.cpp \
		SessionInfoWidget.cpp \
		UpdateCheckDialog.cpp \
		VersionDialog.cpp \
		VideoWidget.cpp

MOC_SRC=	AddNetworkSinkDialog.h \
		AudioWidget.h \
		AvailabilityWidget.h \
		CallButton.h \
		ConfigurationDialog.h \
		ContactEditDialog.h \
		FileTransferAckDialog.h \
		HelpDialog.h \
		IdentityDialog.h \
		MainWindow.h \
		MessageHistory.h \
		MessageInput.h \
		MessageWidget.h \
		OpenVideoAudioPreviewDialog.h \
		OverviewContactsWidget.h \
		OverviewDataStreamsWidget.h \
		OverviewErrorsWidget.h \
		OverviewFileTransfersWidget.h \
		OverviewNetworkStreamsWidget.h \
		OverviewPlaylistWidget.h \
		OverviewThreadsWidget.h \
		ParticipantWidget.h \
		SegmentSelectionDialog.h \
		SessionInfoWidget.h \
		StreamingControlWidget.h \
		StreamPositionWidget.h \
		UpdateCheckDialog.h \
		VersionDialog.h \
		VideoWidget.h

QT_FORMS=	../forms/AddNetworkSinkDialog.ui \
		../forms/AudioWidget.ui \
		../forms/AvailabilityWidget.ui \
		../forms/ConfigurationDialog.ui \
		../forms/ContactEditDialog.ui \
		../forms/FileTransferAckDialog.ui \
		../forms/HelpDialog.ui \
		../forms/IdentityDialog.ui \
		../forms/MainWindow.ui \
		../forms/MessageWidget.ui \
		../forms/OpenVideoAudioPreviewDialog.ui \
		../forms/OverviewContactsWidget.ui \
		../forms/OverviewDataStreamsWidget.ui \
		../forms/OverviewErrorsWidget.ui \
		../forms/OverviewFileTransfersWidget.ui \
		../forms/OverviewNetworkStreamsWidget.ui \
		../forms/OverviewPlaylistWidget.ui \
		../forms/OverviewThreadsWidget.ui \
		../forms/ParticipantWidget.ui \
		../forms/SegmentSelectionDialog.ui \
		../forms/SessionInfoWidget.ui \
		../forms/StreamingControlWidget.ui \
		../forms/StreamPositionWidget.ui \
		../forms/UpdateCheckDialog.ui \
		../forms/VersionDialog.ui \
		../forms/VideoWidget.ui

QRC_FILE=	../resources/Icons.qrc
RCC_DEST_PATH=  `pwd`

SOURCE_DIRS=	../src ../src/Dialogs ../src/Widgets

VPATH=		../include ../include/Widgets ../include/Dialogs $(SOURCE_DIRS)

###############################################################################

include $(TOP_DIR)/HomerMake/qt4.mk
include $(TOP_DIR)/HomerMake/app.mk
