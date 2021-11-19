#include <getopt.h>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include <glib-object.h>
#include <cstdio>

#include "QtAwesome.h"

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
        QDir(defaultSeadriveLogDir()).absoluteFilePath("crash-gui"));
#endif
}

void setupHDPIFix()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
    // enable builtin retina mode
    // http://blog.qt.digia.com/blog/2013/04/25/retina-display-support-for-mac-os-ios-and-x11/
    // https://qt.gitorious.org/qt/qtbase/source/a3cb057c3d5c9ed2c12fb7542065c3d667be38b7:src/gui/image/qicon.cpp#L1028-1043
    qApp->setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  #if defined(Q_OS_WIN32)
    if (!utils::win::fixQtHDPINonIntegerScaling()) {
        qApp->setAttribute(Qt::AA_EnableHighDpiScaling);
    }
  #elif !defined(Q_OS_MAC)
    // Enable HDPI auto detection.
    // See http://blog.qt.io/blog/2016/01/26/high-dpi-support-in-qt-5-6/
    qApp->setAttribute(Qt::AA_EnableHighDpiScaling);
  #endif
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

void handleCommandLineOption(int argc, char *argv[])
{
    int c;
    static const char *short_options = "KDXPc:d:f:";
    static const struct option long_options[] = {
        { "fuse-opts", required_argument, NULL, 'o' },
        { "stop", no_argument, NULL, 'K'},
        { "open-local-file", no_argument, NULL, 'f' },
#if defined(Q_OS_WIN32)
        { "drive-letter", required_argument, NULL, 'L' },
#endif
        { "delay", no_argument, NULL, 'D' },
        { "remove-user-data", no_argument, NULL, 'X' },

        // seadrive-gui --dev won't launch seadrive daemon (you are
        // supposed to launch it yourself). This is for speeding up
        // the development cycles because starting the seadrvie daemon
        // and wait for it to be ready is very slow.
        { "dev", no_argument, NULL, 'E' },
        { NULL, 0, NULL, 0, },
    };

    while ((c = getopt_long (argc, argv, short_options,
                             long_options, NULL)) != EOF) {
        switch (c) {
        case 'o':
            g_setenv ("SEADRIVE_FUSE_OPTS", optarg, 1);
            break;
#if defined(Q_OS_WIN32)
        case 'L':
            g_setenv ("SEADRIVE_LETTER", optarg, 1);
            break;
#if defined(HAVE_SPARKLE_SUPPORT) && defined(SEADRIVE_GUI_DEBUG)
        case 'U':
            g_setenv ("SEADRIVE_APPCAST_URI", optarg, 1);
            break;
#endif
#endif
        case 'D':
            msleep(1000);
            break;
        case 'X':
            do_remove_user_data();
            exit(0);
        case 'f':
            OpenLocalHelper::instance()->handleOpenLocalFromCommandLine(optarg);
            break;
        case 'E':
            dev_mode = true;
            break;
        case 'K':
            // do_stop_app requires gui object be initialized. We save a
            // flag here and exeute it later.
            stop_app = true;
            return;
        default:
            exit(1);
        }
    }

}


} // anonymous namespace

int main(int argc, char *argv[])
{
    srand(time(NULL));
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
    handleCommandLineOption(argc, argv);

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
