TEMPLATE  = lib
TARGET    = quazip

# QuaZip version = 0.4.4

include(../../../libs/libsworkbench.pri)
include($${SOURCES_LIBS_PATH}/utils.pri)

win32:LIBS *= -L$${SOURCES_CONTRIBS_PATH}/quazip
#else:linux*:LIBS *= -L/lib

LIBS *= -lz

DEPENDPATH += .
INCLUDEPATH += .

DEFINES += QUAZIP_BUILD
CONFIG(staticlib): DEFINES += QUAZIP_STATIC

# Input
HEADERS += \
    crypt.h\
    ioapi.h\
    JlCompress.h\
    quaadler32.h\
    quachecksum32.h\
    quacrc32.h\
    quazip.h\
    quazipfile.h\
    quazipfileinfo.h\
    quazipnewinfo.h\
    quazip_global.h\
    unzip.h\
    zip.h\
    global.h

SOURCES += *.c *.cpp
