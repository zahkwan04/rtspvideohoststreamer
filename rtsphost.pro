QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# GStreamer configuration for Windows
win32 {
    # Update these paths to match your GStreamer installation
    GSTREAMER_DIR = C:/gstreamer/1.0/msvc_x86_64

    INCLUDEPATH += \
        $${GSTREAMER_DIR}/include \
        $${GSTREAMER_DIR}/include/gstreamer-1.0 \
        $${GSTREAMER_DIR}/include/glib-2.0 \
        $${GSTREAMER_DIR}/lib/glib-2.0/include

    LIBS += \
        -L$${GSTREAMER_DIR}/lib \
        -lgstreamer-1.0 \
        -lgobject-2.0 \
        -lglib-2.0 \
        -lgstrtsp-1.0 \
        -lgstrtp-1.0 \
        -lgstnet-1.0 \
        -lgstbase-1.0 \
        -lgstapp-1.0 \
        -lgstvideo-1.0
}
