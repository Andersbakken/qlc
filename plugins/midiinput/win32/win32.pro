include(../../../variables.pri)

TEMPLATE = lib
LANGUAGE = C++
TARGET   = midiinput

CONFIG      += plugin
INCLUDEPATH += ../common/src
INCLUDEPATH += ../../interfaces
DEPENDPATH  += ../common/src
LIBS        += -lwinmm

###############################################################################
# Installation
###############################################################################

target.path = $$INSTALLROOT/$$INPUTPLUGINDIR
INSTALLS   += target

###############################################################################
# Sources
###############################################################################

FORMS += ../common/src/configuremidiinput.ui \
         ../common/src/configuremidiline.ui

HEADERS += ../common/src/configuremidiinput.h \
           ../common/src/configuremidiline.h \
           ../common/src/midiinputevent.h \
           ../common/src/midiprotocol.h \
           mididevice.h \
           midiinput.h

SOURCES += ../common/src/configuremidiinput.cpp \
           ../common/src/configuremidiline.cpp \
           ../common/src/midiinputevent.cpp \
           ../common/src/midiprotocol.cpp \
           mididevice.cpp \
           midiinput.cpp

HEADERS += ../../interfaces/qlcinplugin.h
