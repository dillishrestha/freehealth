TEMPLATE         = app
TARGET           = freeddimanager
mac:TARGET       = $$quote(FreeDDIManager)

BUILD_PATH_POSTFIXE = FreeDDIManager

# include general configuration
INSTALL_PROFILES_FILES = 0
INSTALL_DRUGS = 0
INSTALL_ICD_DATABASE = 0
INSTALL_ACCOUNT_FILES = 0
INSTALL_ZIPCODES = 0
INSTALL_EDRC_FILES = 0

include(../../buildspecs/config.pri)
include($${SOURCES_BUILDSPECS_PATH}/install.pri)

# include SDKs
include( $${SOURCES_LIBS_PATH}/extensionsystem.pri )
include( $${SOURCES_LIBS_PATH}/translationutils.pri )
include( $${SOURCES_LIBS_PATH}/utils.pri )
include( $${SOURCES_LIBS_PATH}/rpath.pri )

QT *= sql \
    network \
    xml

DEFINES *= FREEDDDIMANAGER

macx {
  ICON=$${SOURCES_GLOBAL_RESOURCES}/pixmap/svg/freeddimanager.icns
  QMAKE_INFO_PLIST=Info.plist
}
win32:RC_FILE=freeddimanager_win32_icon.rc

# sources
SOURCES += \
    main.cpp \

OTHER_FILES += freeddimanager_win32_icon.rc Info.plist
