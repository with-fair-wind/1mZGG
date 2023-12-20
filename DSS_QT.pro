#-------------------------------------------------
#
# Project created by QtCreator 2019-12-31T09:32:56
#
#-------------------------------------------------

QT       += core gui
QT       += network
QT       += serialport
QT       += charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DSS_QT
TEMPLATE = app

CONFIG += c++11
# CONFIG += console

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS \
            POSIX_HOSTPC

QMAKE_CXXFLAGS_RELEASE+=-O0

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    CommCamera.cpp \
    CommMasterControl.cpp \
    CommServo.cpp \
    Grabber.cpp \
    ImageCode.cpp \
    ImageProcAlgo.cpp \
    NetApp.cpp \
    NetAtmos.cpp \
    NetExchange.cpp \
    NetImageSender.cpp \
    TrackAlgo.cpp \
    UI_CtrlPad.cpp \
    UI_DispPad.cpp \
    UI_DispPadS.cpp \
    UI_InitDlg.cpp \
    WorkerObject.cpp \
        main.cpp \
    SingleApplication.cpp \
    GlobalParameter.cpp \
    StandardThread.cpp \
    CommDisplay.cpp \
    MyLog.cpp \
    CommExposure.cpp \
    QLabelImage.cpp \
    QImage8UC1BufferPackage.cpp \
    ImageStorage.cpp \
    ImageReplayer.cpp \
    ImageProcessor.cpp \
    DeviceManager.cpp \
    Labeler.cpp \
    TrackDataStorage.cpp \
    NetErrorDiagnose.cpp \
    UI_DistCurve.cpp \
    mpolyfit.cpp \
    mfft.cpp \
    getperiod.cpp

HEADERS += \
    CommCamera.h \
    CommMasterControl.h \
    CommServo.h \
    DeviceManager.h \
    Grabber.h \
    ImageCode.h \
    ImageProcAlgo.h \
    Labeler.h \
    NetApp.h \
    NetAtmos.h \
    NetExchange.h \
    NetImageSender.h \
    SingleApplication.h \
    GlobalParameter.h \
    DefinedMacro.h \
    StandardThread.h \
    CommDisplay.h \
    MyLog.h \
    CommExposure.h \
    QLabelImage.h \
    QImage8UC1BufferPackage.h \
    ImageStorage.h \
    ImageReplayer.h \
    ImageProcessor.h \
    TrackAlgo.h \
    TrackDataStorage.h \
    NetErrorDiagnose.h \ \
    UI_CtrlPad.h \
    UI_DispPad.h \
    UI_DispPadS.h \
    UI_DistCurve.h \
    UI_InitDlg.h \
    WorkerObject.h \
    mpolyfit.h \
    mfft.h \
    getperiod.h \
    photometry/ADefine.h \
    photometry/ATimeSpace.h \
    photometry/WinDatatypeDef.h \
    photometry/dllPhotometry_global.h \
    photometry/dllphotometry.h \
    photometry/myTypeDefine.h \
    starmap/starmap.h \
    starmap/starmap_global.h \

INCLUDEPATH +=\
    /usr/local/include/Sapera/include \
    /usr/local/include/Sapera/classes/basic \
    /usr/local/include/Sapera/examples/common \
    /usr/lib/gcc/x86_64-linux-gnu/11/include \
    /usr/local/cuda-11.7/include

#LIBS +=\
#   -L/usr/local/lib -lpthread -lstdc++ -L/usr/X11R6/lib -lXext -lX11 -L/usr/local/lib -lSapera++ -lSaperaLT -lCorW32 -lOpenCL

LIBS += -L/usr/local/lib -lSapera++ -lSaperaLT -lCorW32
LIBS += -L/usr/local/cuda-11.7/lib64 -lOpenCL
LIBS += -L$$PWD/../proLib -lStarMap -ldllPhotometry -lStarPosition


#openmp
QMAKE_CXXFLAGS += -fopenmp
LIBS += -lgomp -lpthread

DISTFILES += \
    KernelCL.cl \

DEPENDPATH += $$PWD/.

FORMS += \
    UI_CtrlPad.ui \
    UI_DispPad.ui \
    UI_DispPadS.ui \
    UI_DistCurve.ui \
    UI_InitDlg.ui

RESOURCES += \
    MainWindow.qrc
