###############################################################################
# Author:  Thomas Volkert
# Since:   2012-05-31
###############################################################################
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakePreDefs.txt)

##############################################################
# Configuration
##############################################################

##############################################################
# find package "QT" and include some macros
MESSAGE("##### Searching for library QT4")
SET (QT_QMAKE_EXECUTABLE $ENV{QT_QMAKE_EXECUTABLE} )
Find_Package(Qt4 4.6 REQUIRED)
IF (NOT QT4_FOUND)
	IF ($ENV{QT_QMAKE_EXECUTABLE} MATCHES "")
		MESSAGE(FATAL_ERROR "Library QT4 not found, try to define path to Qt4 qmake in environment variable QT_QMAKE_EXECUTABLE")
	ELSE()
		MESSAGE(FATAL_ERROR "You need library QT4")
	ENDIF()
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
	../include
	../include/Widgets
	../../Homer/include
	../../Homer/include/Widgets
	../../Homer/include/Dialogs
	../../HomerBase/include
	../../HomerBase/include/Logging
	../../HomerGAPI/include
	../../HomerConference/include
	../../HomerMonitor/include
	../../HomerMultimedia/include
	${QT_INCLUDE_DIR}
	${CMAKE_BINARY_DIR}/HomerNetworkSimulator/libHomerNetworkSimulator
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
SET (FLAGS
	${FLAGS}
	
)

##############################################################
# SOURCES
SET (SOURCES
	../src/Core/ChannelName
	../src/Core/ChannelSetup
	../src/Core/ChannelBinding
	../src/Core/ChannelConnection
	../src/Core/Cep
	../src/Core/Coordinator
	../src/Core/Domain
	../src/Core/DomainNameService
	../src/Core/Link
	../src/Core/Node
	../src/Core/Scenario
	../src/NetworkSimulator
	../src/Widgets/OverviewNetworkSimulationWidget
)
SET (QT_MOC_SOURCES
	../include/Widgets/OverviewNetworkSimulationWidget.h
)
SET (QT_UI_SOURCES
	../forms/Widgets/OverviewNetworkSimulationWidget.ui
)

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
	)
ENDIF()
SET (LIBS_WINDOWS
	HomerBase
	HomerGAPI
	HomerMultimedia
	${QT_LIBRARIES}
	${QT_QTCORE_LIBRARIES}
	${QT_QTGUI_LIBRARIES}
	${QT_QTNETWORK_LIBRARIES}
	${QT_QTXML_LIBRARIES}
	${QT_QTWEBKIT_LIBRARIES}
)
	
# USED LIBRARIES for BSD environment
SET (LIBS_BSD
	HomerBase
	HomerGAPI
	HomerMultimedia
        ${QT_LIBRARIES}
)

# USED LIBRARIES for linux environment
SET (LIBS_LINUX
	HomerBase
	HomerGAPI
	HomerMultimedia
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
		libQtCore.so.4.8.1
		libQtGui.so.4.8.1
		libQtWebKit.so.4.9.0
		libgstapp-0.10.so.0
		libgstbase-0.10.so.0.29.0
		libgstpbutils-0.10.so
		libgstreamer-0.10.so.0
		libgstvideo-0.10.so.0.24.0
		libQtDBus.so.4 
		libQtNetwork.so.4
		libQtXml.so.4  
		libgstapp-0.10.so.0.24.0  
		libgstinterfaces-0.10.so    
		libgstpbutils-0.10.so.0          
		libgstreamer-0.10.so.0.29.0  
		libstdc++.so
		libQtDBus.so.4.8.1
		libQtNetwork.so.4.8.1
		libQtXml.so.4.8.1     
		libgstbase-0.10.so
		libgstinterfaces-0.10.so.0  
		libgstpbutils-0.10.so.0.24.0     
		libgstvideo-0.10.so          
		libstdc++.so.6
	)
ENDIF()

# USED LIBRARIES for apple environment
SET (LIBS_APPLE
	HomerBase
	HomerGAPI
	HomerMultimedia
)
IF (NOT(${BUILD} MATCHES "Default"))
	SET (LIBS_APPLE_INSTALL
		QtCore
		QtGui
		QtNetwork
		QtXml
		QtXmlPatterns
		QtWebKit
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
SET (TARGET_LIB_NAME
	HomerNetworkSimulator
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

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../HomerBuild/CMakeCore.txt)