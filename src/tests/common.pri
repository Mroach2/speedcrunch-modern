DEFINES += SPEEDCRUNCH_VERSION=\\\"1.0\\\"

win32:DEFINES += _USE_MATH_DEFINES
win32:DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _SCL_SECURE_NO_WARNINGS

QT += widgets help
CONFIG += c++11

DEPENDPATH += . \
              .. \
              ../core \
              ../gui \
              ../locale \
              ../math \
              ../resources

INCLUDEPATH += . .. ../math ../core ../gui

HEADERS += ../core/book.h \
           ../core/constants.h \
           ../core/colorscheme.h \
           ../core/evaluator.h \
           ../core/functions.h \
           ../core/session.h \
           ../core/errors.h \
           ../core/manualserver.h \
           ../core/numberformatter.h \
           ../core/pageserver.h \
           ../core/settings.h \
           ../core/opcode.h \
           ../core/sessionhistory.h \
           ../core/variable.h \
           ../core/userfunction.h \
           ../core/userunit.h \
           ../gui/displayformatutils.h \
           ../gui/functiontooltiputils.h \
           ../gui/variablelistwidget.h \
           ../math/floatnum/floatnum/floatcommon.h \
           ../math/floatnum/floatnum/floatconfig.h \
           ../math/floatnum/floatnum/floatconst.h \
           ../math/floatnum/floatnum/floatconvert.h \
           ../math/floatnum/floatnum/floaterf.h \
           ../math/floatnum/floatnum/floatexp.h \
           ../math/floatnum/floatnum/floatgamma.h \
           ../math/floatnum/floatnum/floathmath.h \
           ../math/floatnum/floatnum/floatincgamma.h \
           ../math/floatnum/floatnum/floatio.h \
           ../math/floatnum/floatnum/floatipower.h \
           ../math/floatnum/floatnum/floatlog.h \
           ../math/floatnum/floatnum/floatlogic.h \
           ../math/floatnum/floatnum/floatlong.h \
           ../math/floatnum/floatnum/floatnum.h \
           ../math/floatnum/floatnum/floatpower.h \
           ../math/floatnum/floatnum/floatseries.h \
           ../math/floatnum/floatnum/floattrig.h \
           ../math/hmath.h \
           ../math/number.h \
           ../math/quantity.cpp \
           ../math/rational.h \
           ../core/units.h \
           ../core/unitdisplayformat.h \
           ../gui/manualwindow.h

SOURCES += ../core/book.cpp \
           ../core/constants.cpp \
           ../core/colorscheme.cpp \
           ../core/evaluator.cpp \
           ../core/functions.cpp \
           ../core/mathdsl.cpp \
           ../core/manualserver.cpp \
           ../core/numberformatter.cpp \
           ../core/pageserver.cpp \
           ../core/settings.cpp \
           ../core/session.cpp \
           ../core/sessionhistory.cpp \
           ../core/variable.cpp \
           ../core/userfunction.cpp \
           ../core/userunit.cpp \
           ../gui/displayformatutils.cpp \
           ../core/opcode.cpp \
           ../gui/functiontooltiputils.cpp \
           ../gui/variablelistwidget.cpp \
           ../math/floatnum/floatnum/floatcommon.c \
           ../math/floatnum/floatnum/floatconst.c \
           ../math/floatnum/floatnum/floatconvert.c \
           ../math/floatnum/floatnum/floaterf.c \
           ../math/floatnum/floatnum/floatexp.c \
           ../math/floatnum/floatnum/floatgamma.c \
           ../math/floatnum/floatnum/floathmath.c \
           ../math/floatnum/floatnum/floatio.c \
           ../math/floatnum/floatnum/floatipower.c \
           ../math/floatnum/floatnum/floatlog.c \
           ../math/floatnum/floatnum/floatlogic.c \
           ../math/floatnum/floatnum/floatlong.c \
           ../math/floatnum/floatnum/floatnum.c \
           ../math/floatnum/floatnum/floatpower.c \
           ../math/floatnum/floatnum/floatseries.c \
           ../math/floatnum/floatnum/floattrig.c \
           ../math/floatnum/floatnum/floatincgamma.c \
           ../math/hmath.cpp \
           ../math/number.c \
           ../math/cmath.cpp \
           ../math/cnumberparser.cpp \
           ../math/quantity.cpp \
           ../math/rational.cpp \
           ../core/units.cpp \
           ../core/unitdisplayformat.cpp \
           ../gui/manualwindow.cpp
