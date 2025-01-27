#pragma once

#ifdef _WIN32
#include <WinSDKVer.h>
#define WINVER 0x0500
#define _WIN32_WINDOWS 0x0500
#define _WIN32_WINNT 0x0600
#define _ATL_XP_TARGETING
#include <SDKDDKVer.h>
#include <Windows.h>
#endif //_WIN32

#include <thread>
#include <array>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <type_traits>
#include <map>
#include <set>
#include <vector>
#include <stack>
#include <deque>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <atomic>
#include <codecvt>
#include <limits>
#include <cmath>
#include <mutex>
#include <numeric>
#include <fstream>
#include <stdint.h>
#include <condition_variable>
#include <queue>
#include <string_view>
#include <optional>
#include <chrono>
#include <random>
#include <complex>

#include <inttypes.h>


#if defined __APPLE__ || defined __linux__
#define  NO_ASSERTS
#endif //__APPLE__ _LINUX_
#include <cassert>
#if defined NO_ASSERTS
#define assert(e) { if (!(e)) puts("ASSERT: " #e); }
#define RAPIDJSON_ASSERT(e) assert(e)
#endif //NO_ASSERT

#if defined (_WIN32)
#include <tchar.h>
#include <strsafe.h>
#include <strsafe.h>
#include <sal.h>
#include <Psapi.h>
#include <ObjBase.h>
#include <ShObjIdl.h>
#include <propvarutil.h>
#include <functiondiscoverykeys.h>
#include <intsafe.h>
#include <guiddef.h>
#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>

#define QT_WIDGETS_LIB
#define QT_GUI_LIB
#include <QResource>
#include <QTranslator>
#include <QScreen>
#include <QtPlugin>
#include <QLibrary>
#include <QGuiApplication>
#include <QShowEvent>
#include <QComboBox>
#include <QMainWindow>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include <QListWidget>
#include <QPaintEvent>
#include <QBitmap>
#include <QLinearGradient>
#include <QGraphicsOpacityEffect>
#include <QGraphicsBlurEffect>
#include <QCommonStyle>
#include <QListView>
#include <QLabel>
#include <QSizePolicy>
#include <QFont>
#include <QFile>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QImage>
#include <QList>
#include <QString>
#include <QObject>
#include <QTime>
#include <QStringList>
#include <QTimer>
#include <QItemDelegate>
#include <QAbstractListModel>
#include <QHash>
#include <QApplication>
#include <QDockWidget>
#include <QSize>
#include <QDate>
#include <QMutex>
#include <QScrollBar>
#include <QtConcurrent/QtConcurrent>
#include <QMap>
#include <QTextStream>
#include <QWidget>
#include <QTreeView>
#include <QBoxLayout>
#include <QStackedLayout>
#include <QHeaderView>
#include <QCompleter>
#include <QStandardItemModel>
#include <QPainter>
#include <QLineEdit>
#include <QKeyEvent>
#include <QTextEdit>
#include <QMetaType>
#include <QVariant>
#include <QDateTime>
#include <qframe.h>
#include <QDesktopWidget>
#include <QTextFrame>
#include <QToolTip>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableView>
#include <QMapIterator>
#include <QScroller>
#include <QAbstractTextDocumentLayout>
#include <QFileDialog>
#include <QTextDocumentFragment>
#include <QPixmapCache>
#include <QTextBrowser>
#include <QClipboard>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QProxyStyle>
#include <QDesktopServices>
#include <QCheckBox>
#include <QGraphicsView>
#include <QtMultimediaWidgets/QGraphicsVideoItem>
#include <QDesktopWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMovie>
#include <QGestureEvent>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtEndian>
#include <QDrag>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QGlWidget>
#include <QOpenglWidget.h>
#include <QOpenglWindow.h>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QStringBuilder>
#include <QSvgRenderer>
#include <QPointer>
#include <QVariantMap>
#include <QValidator>
#include <QShortcut>
#include <QLoggingCategory>
#include <QCryptographicHash>
#include <QSplitter>
#include <QTabWidget>
#include <QCollator>
#include <QTemporaryFile>
#include <QScopedValueRollback>
#if defined(IM_AUTO_TESTING)
    #include <QTest>
#endif

#undef max
#undef MAX
#undef min
#undef MIN
#undef small

#elif defined (__linux__)

#include <QtCore/qresource.h>
#include <QtCore/qtranslator.h>
#include <QtGui/qscreen.h>
#include <QtCore/qplugin.h>
#include <QtCore/qlibrary.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qdockwidget.h>
#include <QtGui/qbitmap.h>
#include <QtGui/qbrush.h>
#include <QtWidgets/qgraphicseffect.h>
#include <QtWidgets/qcommonstyle.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qsizepolicy.h>
#include <QtGui/qfont.h>
#include <QtCore/qfile.h>
#include <QtCore/qxmlstream.h>
#include <QtCore/qbuffer.h>
#include <QtGui/qimage.h>
#include <QtCore/qlist.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qobject.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdatastream.h>
#include <QtWidgets/qitemdelegate.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qhash.h>
#include <QtWidgets/qapplication.h>
#include <QtCore/qsize.h>
#include <QtCore/qmutex.h>
#include <QtWidgets/qscrollbar.h>
#include <QtConcurrent/qtconcurrentcompilertest.h>
#include <QtConcurrent/qtconcurrentexception.h>
#include <QtConcurrent/qtconcurrentfilter.h>
#include <QtConcurrent/qtconcurrentfilterkernel.h>
#include <QtConcurrent/qtconcurrentfunctionwrappers.h>
#include <QtConcurrent/qtconcurrentiteratekernel.h>
#include <QtConcurrent/qtconcurrentmap.h>
#include <QtConcurrent/qtconcurrentmapkernel.h>
#include <QtConcurrent/qtconcurrentmedian.h>
#include <QtConcurrent/qtconcurrentreducekernel.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <QtConcurrent/qtconcurrentrunbase.h>
#include <QtConcurrent/qtconcurrentstoredfunctioncall.h>
#include <QtConcurrent/qtconcurrentthreadengine.h>
#include <QtConcurrent/qtconcurrentversion.h>
#include <QtCore/qmap.h>
#include <QtCore/qtextstream.h>
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qstackedlayout.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qcompleter.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qtextedit.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qvariant.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qdesktopwidget.h>
#include <QtWidgets/qtooltip.h>
#include <QtWidgets/qscrollarea.h>
#include <QtWidgets/qstackedwidget.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qscroller.h>
#include <QtGui/qabstracttextdocumentlayout.h>
#include <QtWidgets/qfiledialog.h>
#include <QtGui/qtextdocumentfragment.h>
#include <QtGui/qpixmapcache.h>
#include <QtWidgets/qtextbrowser.h>
#include <QtWidgets/qdesktopwidget.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qpropertyanimation.h>
#include <QtGui/qtextobject.h>
#include <QtCore/qmimedata.h>
#include <QtCore/qmimetype.h>
#include <QtCore/qmimedatabase.h>
#include <QtWidgets/qcheckbox.h>
#include <QtCore/qabstractnativeeventfilter.h>
#include <QtGui/qdesktopservices.h>
#include <QtGui/qtextcursor.h>
#include <QtCore/qprocess.h>
#include <QtWidgets/qproxystyle.h>
#include <QtWidgets/qdesktopwidget.h>
#include <QtWidgets/qsystemtrayicon.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtGui/qclipboard.h>
#include <QtGui/qmovie.h>
#include <QtWidgets/qgesture.h>
#include <QtCore/quuid.h>
#include <QtCore/qurlquery.h>
#include <QtCore/qendian.h>
#include <QtGui/qdrag.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtCore/qstringbuilder.h>
#include <QtCore/qpointer.h>
#include <QtCore/qpauseanimation.h>
#include <QtCore/qvariant.h>
#include <QtSvg/qsvgrenderer.h>
#include <QtWidgets/qshortcut.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qcryptographichash.h>
#include <QtWidgets/qsplitter.h>
#include <QtWidgets/qtabwidget.h>
#include <QtCore/qcollator.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qscopedvaluerollback.h>
#else
#include "macconfig.h"
#ifndef HOCKEY_APPID
#define HOCKEY_APPID @"13f40ef63c02469c93f566cc2c116952"
#endif
#import <QtCore/qresource.h>
#import <QtCore/qtranslator.h>
#import <QtGui/qscreen.h>
#import <QtCore/qplugin.h>
#import <QtCore/qlibrary.h>
#import <QtGui/qguiapplication.h>
#import <QtGui/qevent.h>
#import <QtWidgets/qcombobox.h>
#import <QtWidgets/qmainwindow.h>
#import <QtWidgets/qpushbutton.h>
#import <QtWidgets/qstyleoption.h>
#import <QtWidgets/qlistwidget.h>
#import <QtGui/qbitmap.h>
#import <QtGui/qbrush.h>
#import <QtWidgets/qgraphicseffect.h>
#import <QtWidgets/qcommonstyle.h>
#import <QtWidgets/qlistview.h>
#import <QtWidgets/qlabel.h>
#import <QtWidgets/qsizepolicy.h>
#import <QtGui/qfont.h>
#import <QtCore/qfile.h>
#import <QtCore/qxmlstream.h>
#import <QtCore/qbuffer.h>
#import <QtGui/qimage.h>
#import <QtCore/qlist.h>
#import <QtCore/qglobal.h>
#import <QtCore/qstring.h>
#import <QtCore/qobject.h>
#import <QtCore/qdatetime.h>
#import <QtCore/qstringlist.h>
#import <QtCore/qtimer.h>
#import <QtCore/qdatastream.h>
#import <QtWidgets/qitemdelegate.h>
#import <QtCore/qabstractitemmodel.h>
#import <QtCore/qhash.h>
#import <QtWidgets/qapplication.h>
#import <QtCore/qsize.h>
#import <QtCore/qmutex.h>
#import <QtWidgets/qscrollbar.h>
#import <QtConcurrent/qtconcurrentcompilertest.h>
#import <QtConcurrent/qtconcurrentexception.h>
#import <QtConcurrent/qtconcurrentfilter.h>
#import <QtConcurrent/qtconcurrentfilterkernel.h>
#import <QtConcurrent/qtconcurrentfunctionwrappers.h>
#import <QtConcurrent/qtconcurrentiteratekernel.h>
#import <QtConcurrent/qtconcurrentmap.h>
#import <QtConcurrent/qtconcurrentmapkernel.h>
#import <QtConcurrent/qtconcurrentmedian.h>
#import <QtConcurrent/qtconcurrentreducekernel.h>
#import <QtConcurrent/qtconcurrentrun.h>
#import <QtConcurrent/qtconcurrentrunbase.h>
#import <QtConcurrent/qtconcurrentstoredfunctioncall.h>
#import <QtConcurrent/qtconcurrentthreadengine.h>
#import <QtConcurrent/qtconcurrentversion.h>
#import <QtCore/qmap.h>
#import <QtCore/qtextstream.h>
#import <QtWidgets/qwidget.h>
#import <QtWidgets/qtreeview.h>
#import <QtWidgets/qboxlayout.h>
#import <QtWidgets/qstackedlayout.h>
#import <QtWidgets/qheaderview.h>
#import <QtWidgets/qcompleter.h>
#import <QtGui/qstandarditemmodel.h>
#import <QtGui/qpainter.h>
#import <QtWidgets/qlineedit.h>
#import <QtWidgets/qtextedit.h>
#import <QtCore/qmetatype.h>
#import <QtCore/qvariant.h>
#import <QtWidgets/qframe.h>
#import <QtWidgets/qdesktopwidget.h>
#import <QtWidgets/qtooltip.h>
#import <QtWidgets/qscrollarea.h>
#import <QtWidgets/qstackedwidget.h>
#import <QtWidgets/qtableview.h>
#import <QtWidgets/qscroller.h>
#import <QtGui/qabstracttextdocumentlayout.h>
#import <QtWidgets/qfiledialog.h>
#import <QtGui/qtextdocumentfragment.h>
#import <QtGui/qpixmapcache.h>
#import <QtWidgets/qtextbrowser.h>
#import <QtWidgets/qdesktopwidget.h>
#import <QtCore/qeasingcurve.h>
#import <QtCore/qpropertyanimation.h>
#import <QtGui/qtextobject.h>
#import <QtCore/qmimedata.h>
#import <QtCore/qmimetype.h>
#import <QtCore/qmimedatabase.h>
#import <QtWidgets/qcheckbox.h>
#import <QtCore/qabstractnativeeventfilter.h>
#import <QtGui/qdesktopservices.h>
#import <QtGui/qtextcursor.h>
#import <QtCore/qprocess.h>
#import <QtWidgets/qproxystyle.h>
#import <QtWidgets/qdesktopwidget.h>
#import <QtWidgets/qsystemtrayicon.h>
#import <QtWidgets/qmenu.h>
#import <QtWidgets/qmenubar.h>
#import <QtWidgets/qdockwidget.h>
#import <QtGui/qclipboard.h>
#import <QtCore/qobjectcleanuphandler.h>
#import <QtGui/qmovie.h>
#import <QtWidgets/qgesture.h>
#import <QtCore/quuid.h>
#import <QtCore/qurlquery.h>
#import <QtMultimedia/qmediaplayer.h>
#import <QtCore/qendian.h>
#import <QtGui/qdrag.h>
#import <QtGui/qopengltexture.h>
#import <QtGui/qopenglshaderprogram.h>
//#import <QtOpenGL/QGLWidget>
#import <QtWidgets/qopenglwidget.h>
#import <QtGui/qopenglwindow.h>
#import <QtGui/qopenglfunctions.h>
#import <QtGui/qopenglbuffer.h>
#import <QtWidgets/qgraphicsscene.h>
#import <QtWidgets/qgraphicsitem.h>
#import <QtMacExtras/qmacfunctions.h>
#import <QtCore/qstringbuilder.h>
#import <QtCore/qpointer.h>
#import <QtCore/qpauseanimation.h>
#import <QtCore/qvariant.h>
#import <QtSvg/qsvgrenderer.h>
#import <QtWidgets/qshortcut.h>
#import <QtCore/qloggingcategory.h>
#import <QtCore/qcryptographichash.h>
#import <QtWidgets/qsplitter.h>
#import <QtWidgets/qtabwidget.h>
#import <QtCore/qcollator.h>
#import <QtCore/qtemporaryfile.h>
#import <QtCore/qscopedvaluerollback.h>
#endif // _WIN32

#include "../common.shared/typedefs.h"
#include "../common.shared/common.h"

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#define qsl(x) QStringLiteral(x)
#define ql1s(x) QLatin1String(x)
#define ql1c(x) QLatin1Char(x)

#include "../gui.shared/product.h"
#include "utils/translator.h"
#include "../gui.shared/constants.h"
#include "../common.shared/constants.h"
#include "../gui.shared/TestingTools.h"

#ifndef __STRING_COMPARATOR__
#define __STRING_COMPARATOR__

struct StringComparator
{
    using is_transparent = std::true_type;

    bool operator()(const QString& lhs, const QString& rhs) const { return lhs < rhs; }
    bool operator()(const QStringRef& lhs, const QString& rhs) const { return lhs.compare(rhs) < 0; }
    bool operator()(const QStringRef& lhs, const QStringRef& rhs) const { return lhs < rhs; }
    bool operator()(const QString& lhs, const QStringRef& rhs) const { return lhs.compare(rhs) < 0; }
    bool operator()(const QString& lhs, QLatin1String rhs) const { return lhs < rhs; }
    bool operator()(QLatin1String lhs, const QString& rhs) const { return lhs < rhs; }
    bool operator()(QLatin1String lhs, QLatin1String rhs) const { return lhs < rhs; }
    bool operator()(QLatin1String lhs, const QStringRef& rhs) const { return rhs.compare(lhs) > 0; }
    bool operator()(const QStringRef& lhs, QLatin1String rhs) const { return lhs.compare(rhs) < 0; }

    template<typename T>
    bool operator()(const char* lhs, const T& rhs) const { return operator()(ql1s(lhs), rhs); }

    template<typename T>
    bool operator()(const T& lhs, const char* rhs) const { return operator()(lhs, ql1s(rhs)); }

    bool operator()(const char* lhs, const char* rhs) const { return operator()(ql1s(lhs), ql1s(rhs)); }
};
#endif // __STRING_COMPARATOR__

namespace openal
{
#include <AL/al.h>
#include <AL/alc.h>
}
