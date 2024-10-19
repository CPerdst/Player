QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    framequeue.cpp \
    main.cpp \
    mediaplayer.cpp \
    packetqueue.cpp \
    playerdialog.cpp

HEADERS += \
    framequeue.h \
    mediaplayer.h \
    packetqueue.h \
    playerdialog.h

FORMS += \
    playerdialog.ui

# INCLUDEPATH += $$PWD/ffmpeg-4.2.2/include \
#     $$PWD/SDL2-2.0.10/include \

INCLUDEPATH += $$PWD/ffmpeg-4.2.2/include \
      $$PWD/SDL2-2.0.10/include

LIBS += $$PWD/ffmpeg-4.2.2/lib/avcodec.lib\
     $$PWD/ffmpeg-4.2.2/lib/avdevice.lib\
     $$PWD/ffmpeg-4.2.2/lib/avfilter.lib\
     $$PWD/ffmpeg-4.2.2/lib/avformat.lib\
     $$PWD/ffmpeg-4.2.2/lib/avutil.lib\
     $$PWD/ffmpeg-4.2.2/lib/postproc.lib\
     $$PWD/ffmpeg-4.2.2/lib/swresample.lib\
     $$PWD/ffmpeg-4.2.2/lib/swscale.lib\
     $$PWD/SDL2-2.0.10/lib/x86/SDL2.lib\
     $$PWD/ffmpeg-4.2.2/lib/avcodec.lib

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
