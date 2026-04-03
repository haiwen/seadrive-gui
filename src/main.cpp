#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include <glib-object.h>
#include <cstdio>

#include "QtAwesome.h"

#include "account.h"
#include "utils/utils.h"
#include "utils/process.h"
#include "seadrive-gui.h"
#include "open-local-helper.h"

#if defined(Q_OS_WIN32)
#include "utils/utils-win.h"
#endif
#if defined(Q_OS_MAC)
#include "application.h"
#include "utils/utils-mac.h"
#endif

#include "i18n.h"
#if defined(SEADRIVE_CLIENT_HAS_CRASH_REPORTER)
#include "crash-handler.h"
#endif // SEADRIVE_CLIENT_HAS_CRASH_REPORTER
#include "utils/utils.h"
#include "utils/uninstall-helpers.h"

namespace {

const char *appName = "seadrive-gui";

#if defined(_MSC_VER)
const char *seadriveName = "seadrive.exe";
#endif

bool dev_mode = false;
bool stop_app = false;

void initGlib()
{
#if !GLIB_CHECK_VERSION(2, 35, 0)
    g_type_init();
#endif
#if !GLIB_CHECK_VERSION(2, 31, 0)
    g_thread_init(NULL);
#endif
}

void initBreakpad()
{
#ifdef SEADRIVE_CLIENT_HAS_CRASH_REPORTER
    // if we have built with breakpad, load it in run time
    Breakpad::CrashHandler::instance()->Init(
        QDir(seadriveLogDir()).absoluteFilePath("crash-gui"));
#endif
}

void setupHDPIFix()
{
    // enable builtin retina mode
    // http://blog.qt.digia.com/blog/2013/04/25/retina-display-support-for-mac-os-ios-and-x11/
    // https://qt.gitorious.org/qt/qtbase/source/a3cb057c3d5c9ed2c12fb7542065c3d667be38b7:src/gui/image/qicon.cpp#L1028-1043
    qApp->setAttribute(Qt::AA_UseHighDpiPixmaps);

  #if defined(Q_OS_WIN32)
    if (!utils::win::fixQtHDPINonIntegerScaling()) {
        qApp->setAttribute(Qt::AA_EnableHighDpiScaling);
    }
  #elif !defined(Q_OS_MAC)
    // Enable HDPI auto detection.
    // See http://blog.qt.io/blog/2016/01/26/high-dpi-support-in-qt-5-6/
    qApp->setAttribute(Qt::AA_EnableHighDpiScaling);
  #endif
}

void setupSettingDomain()
{
    // see QSettings documentation
    QCoreApplication::setOrganizationName(getBrand());
    QCoreApplication::setOrganizationDomain("seafile.com");
    QString appName = getBrand();

    // Special treatment to keep consistent with old versions. Otherwise the
    // existing settings would be lost.
    if (appName == "SeaDrive") {
        appName = "Seafile Drive";
    }
    QCoreApplication::setApplicationName(QString("%1 Client").arg(appName));
}

void handleCommandLineOption(const QApplication &app)
{
    QCommandLineParser parser;

    parser.addOptions({
        QCommandLineOption(QStringList({"D", "delay"})),
        // seadrive-gui --dev won't launch seadrive daemon (you are
        // supposed to launch it yourself). This is for speeding up
        // the development cycles because starting the seadrvie daemon
        // and wait for it to be ready is very slow.
        QCommandLineOption(QStringList({"E", "dev"})),
        QCommandLineOption(QStringList({"K", "stop"})),
#ifdef Q_OS_WIN32
        QCommandLineOption(QStringList({"L", "drive-letter"}), "The drive letter", "letter"),
#endif
        QCommandLineOption(QStringList({"X", "remove-user-data"})),
        QCommandLineOption(QStringList({"f", "open-local-file"}), "Open a local file", "file"),
        QCommandLineOption(QStringList({"o", "fuse-opts"}), "FUSE options", "fuse")
    });
    parser.process(app);

    if (parser.isSet("D")) {
        msleep(1000);
    }
    if (parser.isSet("E")) {
        dev_mode = true;
    }
    if (parser.isSet("K")) {
        // do_stop_app requires gui object be initialized. We save a
        // flag here and exeute it later.
        stop_app = true;
    }
#ifdef Q_OS_WIN32
    if (parser.isSet("L")) {
        QByteArray path = parser.value("L").toUtf8();
        g_setenv("SEADRIVE_LETTER", path.constData(), 1);
    }
#endif
    if (parser.isSet("o")) {
        QByteArray path = parser.value("o").toUtf8();
        g_setenv("SEADRIVE_FUSE_OPTS", path.constData(), 1);
    }
    if (parser.isSet("X")) {
        do_remove_user_data();
        exit(0);
    }
    if (parser.isSet("f")) {
        OpenLocalHelper::instance()->handleOpenLocalFromCommandLine(parser.value("f"));
    }
}

} // anonymous namespace

int main(int argc, char *argv[])
{
    srand(time(NULL));

    qRegisterMetaType<Account>();

    // On Mac, we use the file provider container directory as the default data
    // location. The container directory path is too deep that it exceeds the
    // limit of unix domain socket path in libsearpc. So we set the current
    // working directory to the home and uses relative path to access the data
    // location.
    QDir::setCurrent(seadriveWorkDir());

#if defined(Q_OS_MAC)
    Application app(argc, argv);
#else
    QApplication app(argc, argv);
#endif
    app.setQuitOnLastWindowClosed(false);

    // call glib's init functions
    initGlib();

#if defined(SEADRIVE_CLIENT_HAS_CRASH_REPORTER)
#if defined(Q_OS_WIN32)
    initBreakpad();
#endif // Q_OS_WIN32
#endif // SEADRIVE_CLIENT_HAS_CRASH_REPORTER

    setupHDPIFix();

    // set the domains of settings
    setupSettingDomain();

    // initialize i18n settings
    I18NHelper::getInstance()->init();

    // check seadrive is running
    // start applet
    handleCommandLineOption(app);

    SeadriveGui mGui(dev_mode);
    gui = &mGui;


#if defined(_MSC_VER)
    if (count_process(seadriveName) > 0) {
       QProcess p;
       QString cmd = QString("taskkill /im %1 /f").arg(seadriveName);
       p.execute(cmd);
       p.close();
    }
#endif // _MSC_VER


    if (stop_app) {
        do_stop_app();
        exit(0);
    }

    if (count_process(appName) > 1) {
        QMessageBox::warning(NULL, getBrand(),
                             QObject::tr("%1 Client is already running").arg(getBrand()),
                             QMessageBox::Ok);
        return -1;
    }
    // init qtawesome component
    awesome = new QtAwesome(qApp);
    awesome->initFontAwesome();

    QTimer::singleShot(0, gui, SLOT(start()));

    // start qt eventloop
    int ret = app.exec();

    qWarning("app event loop exited with %d\n", ret);

    return ret;
}
