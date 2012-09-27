!with-webcam {
    error(Wrong configuration, please use CONFIG+=with-webcam to compile the WebCam plugin)
}

TARGET = Webcam
TEMPLATE = lib
message(Building WebCam plugin)

BUILD_PATH_POSTFIXE = FreeMedForms
PROVIDER = FreeMedForms

DEFINES += WEBCAM_LIBRARY

include(../fmf_plugins.pri)
include(webcam_dependencies.pri)

HEADERS = \
    webcamplugin.h \
    webcam_exporter.h \
    webcamconstants.h \
    webcamphotoprovider.h \
    opencvwidget.h \
    webcamdialog.h \
    webcampreferences.h \
    webcamdevice.h

SOURCES += \
    webcamplugin.cpp \
    webcamphotoprovider.cpp \
    opencvwidget.cpp \
    webcamdialog.cpp \
    webcampreferences.cpp \
    webcamdevice.cpp

FORMS += \
    webcamdialog.ui \
    webcampreferences.ui

TRANSLATIONS += $${SOURCES_TRANSLATIONS_PATH}/webcam_fr.ts \
                $${SOURCES_TRANSLATIONS_PATH}/webcam_de.ts \
                $${SOURCES_TRANSLATIONS_PATH}/webcam_es.ts

OTHER_FILES = \
    Webcam.pluginspec

#RESOURCES += \
#    opencv.qrc

