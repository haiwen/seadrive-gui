#include <getopt.h>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include <glib-object.h>
#include <cstdio>

#include "seadrive-gui.h"
#if defined(Q_OS_MAC)
#include "application.h"
#include "utils/utils-mac.h"
#endif

namespace {

void initGlib()
{
#if !GLIB_CHECK_VERSION(2, 35, 0)
    g_type_init();
#endif
#if !GLIB_CHECK_VERSION(2, 31, 0)
    g_thread_init(NULL);
#endif
}

void setupSettingDomain()
{
    // see QSettings documentation
    QCoreApplication::setOrganizationName("Seafile");
    QCoreApplication::setOrganizationDomain("seafile.com");
    QCoreApplication::setApplicationName(QString("Seafile Drive"));
}

void handleCommandLineOption(int argc, char *argv[])
{
    int c;
    static const char *short_options = "o:L:";
    static const struct option long_options[] = {
        { "fuse-opts", required_argument, NULL, 'o' },
#if defined(Q_OS_WIN32)
        { "drive-letter", required_argument, NULL, 'L' },
#endif
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
#endif
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

    // set the domains of settings
    setupSettingDomain();

    handleCommandLineOption(argc, argv);

    // start applet
    SeadriveGui mGui;
    gui = &mGui;
    QTimer::singleShot(0, gui, SLOT(start()));

    // start qt eventloop
    int ret = app.exec();

    qWarning("app event loop exited with %d\n", ret);

    return ret;
}
