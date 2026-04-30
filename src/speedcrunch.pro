equals(QT_MAJOR_VERSION, 6) {
    lessThan(QT_MINOR_VERSION, 4) {
        error(Qt 6.4 or newer is required but version $$[QT_VERSION] was detected.)
    }
}

QT += widgets network
CONFIG += c++17
QMAKE_CXXFLAGS += "-Wall -pedantic"

CONFIG(debug, debug|release) {
    DEFINES += EVALUATOR_DEBUG
}
else {
    DEFINES += QT_NO_DEBUG_OUTPUT
    DEFINES += QT_NO_INFO_OUTPUT
    DEFINES += QT_NO_WARNING_OUTPUT
}

win32-g++:QMAKE_LFLAGS += -static

DEFINES += SPEEDCRUNCH_VERSION=\\\"1.0\\\"
DEFINES += QT_USE_QSTRINGBUILDER
win32:DEFINES += _USE_MATH_DEFINES
win32:DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _SCL_SECURE_NO_WARNINGS

TEMPLATE = app
TARGET = speedcrunch
QT += help

DEPENDPATH += . \
              core \
              gui \
              locale \
              math \
              resources

INCLUDEPATH += . math core gui

win32:RC_FILE = resources/speedcrunch.rc
win32-msvc*:LIBS += User32.lib
!macx {
    !win32 {
        DEPENDPATH += thirdparty
        INCLUDEPATH += thirdparty
        target.path = "/bin"
        menu.path = "/share/applications"
        appdata.path = "/share/appdata"
        icon.path = "/share/pixmaps"
        icon.files += resources/speedcrunch.png
        menu.files += ../pkg/speedcrunch.desktop
        appdata.files += ../pkg/speedcrunch.appdata.xml
        INSTALLS += target icon menu appdata
    }
}

macx {
    ICON = resources/speedcrunch.icns
    QMAKE_INFO_PLIST = ../pkg/Info.plist
    TARGET = SpeedCrunch
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.8
    QMAKE_CXXFLAGS += -std=c++17
}


HEADERS += core/book.h \
           core/constants.h \
           core/evaluator.h \
           core/functions.h \
           core/session.h \
           core/errors.h \
           core/numberformatter.h \
           core/manualserver.h\
           core/pageserver.h \
           core/settings.h \
           core/opcode.h \
           core/sessionhistory.h \
           core/sessionjsonkeys.h \
           core/startupdefinitions.h \
           core/variable.h \
           core/userfunction.h \
           core/userunit.h \
           gui/aboutbox.h \
           gui/bitfieldwidget.h \
           gui/bookdock.h \
           gui/constantswidget.h \
           gui/customkeypaddialog.h \
           gui/displayformatutils.h \
           gui/resultdisplay.h \
           gui/editor.h \
           gui/functiontooltiputils.h \
           gui/functionswidget.h \
           gui/historywidget.h \
           gui/genericdock.h \
           gui/keypad.h \
           gui/variablelistwidget.h \
           gui/userfunctionlistwidget.h \
           gui/userunitlistwidget.h \
           gui/versioncheck.h \
           gui/manualwindow.h \
           gui/notationandprecisiondialog.h \
           gui/numberformatdialog.h \
           gui/mainwindow.h \
           gui/syntaxhighlighter.h \
           math/cmath.h \
           math/floatcommon.h \
           math/floatconfig.h \
           math/floatconst.h \
           math/floatconvert.h \
           math/floaterf.h \
           math/floatexp.h \
           math/floatgamma.h \
           math/floathmath.h \
           math/floatincgamma.h \
           math/floatio.h \
           math/floatipower.h \
           math/floatlog.h \
           math/floatlogic.h \
           math/floatlong.h \
           math/floatnum.h \
           math/floatpower.h \
           math/floatseries.h \
           math/floattrig.h \
           math/hmath.h \
           math/number.h \
           math/quantity.h \
           math/rational.h \
           core/units.h \
           core/unitdisplayformat.h


SOURCES += main.cpp \
           core/book.cpp \
           core/constants.cpp \
           core/evaluator.cpp \
           core/functions.cpp \
           core/mathdsl.cpp \
           core/numberformatter.cpp \
           core/manualserver.cpp\
           core/pageserver.cpp \
           core/settings.cpp \
           core/session.cpp \
           core/sessionhistory.cpp \
           core/startupdefinitions.cpp \
           core/variable.cpp \
           core/userfunction.cpp \
           core/userunit.cpp \
           core/opcode.cpp \
           gui/aboutbox.cpp \
           gui/bitfieldwidget.cpp \
           gui/bookdock.cpp \
           gui/constantswidget.cpp \
           gui/customkeypaddialog.cpp \
           gui/displayformatutils.cpp \
           gui/resultdisplay.cpp \
           gui/editor.cpp \
           gui/functiontooltiputils.cpp \
           gui/functionswidget.cpp \
           gui/historywidget.cpp \
           gui/genericdock.h \
           gui/keypad.cpp \
           gui/syntaxhighlighter.cpp \
           gui/variablelistwidget.cpp \
           gui/userfunctionlistwidget.cpp \
           gui/userunitlistwidget.cpp \
           gui/versioncheck.cpp \
           gui/mainwindow.cpp \
           gui/manualwindow.cpp \
           gui/notationandprecisiondialog.cpp \
           gui/numberformatdialog.cpp \
           math/floatcommon.c \
           math/floatconst.c \
           math/floatconvert.c \
           math/floaterf.c \
           math/floatexp.c \
           math/floatgamma.c \
           math/floathmath.c \
           math/floatio.c \
           math/floatipower.c \
           math/floatlog.c \
           math/floatlogic.c \
           math/floatlong.c \
           math/floatnum.c \
           math/floatpower.c \
           math/floatseries.c \
           math/floattrig.c \
           math/floatincgamma.c \
           math/hmath.cpp \
           math/number.c \
           math/cmath.cpp \
           math/cnumberparser.cpp \
           math/quantity.cpp \
           math/rational.cpp \
           core/units.cpp \
           core/unitdisplayformat.cpp

RESOURCES += resources/speedcrunch.qrc ../doc/build_html_embedded/manual.qrc
TRANSLATIONS += resources/locale/ar.ts \
                resources/locale/bg.ts \
                resources/locale/bn.ts \
                resources/locale/ca.ts \
                resources/locale/cs.ts \
                resources/locale/da.ts \
                resources/locale/de.ts \
                resources/locale/el.ts \
                resources/locale/en_GB.ts \
                resources/locale/en_US.ts \
                resources/locale/es.ts \
                resources/locale/es_ES.ts \
                resources/locale/et.ts \
                resources/locale/eu.ts \
                resources/locale/fa.ts \
                resources/locale/fi.ts \
                resources/locale/fr.ts \
                resources/locale/gl.ts \
                resources/locale/he.ts \
                resources/locale/hi.ts \
                resources/locale/hu.ts \
                resources/locale/id.ts \
                resources/locale/it.ts \
                resources/locale/ja.ts \
                resources/locale/ko.ts \
                resources/locale/lt.ts \
                resources/locale/lv.ts \
                resources/locale/ms.ts \
                resources/locale/nb.ts \
                resources/locale/nl.ts \
                resources/locale/pl.ts \
                resources/locale/pt_BR.ts \
                resources/locale/pt_PT.ts \
                resources/locale/ro.ts \
                resources/locale/ru.ts \
                resources/locale/sk.ts \
                resources/locale/sl.ts \
                resources/locale/sw.ts \
                resources/locale/sv.ts \
                resources/locale/th.ts \
                resources/locale/ta.ts \
                resources/locale/te.ts \
                resources/locale/tr.ts \
                resources/locale/uk.ts \
                resources/locale/uz_Latn.ts \
                resources/locale/vi.ts \
                resources/locale/zh_CN.ts \
                resources/locale/zh_TW.ts
