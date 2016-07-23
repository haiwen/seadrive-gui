#include <getopt.h>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include <glib-object.h>
#include <cstdio>

#include "seadrive-gui.h"

#include "i18n.h"
#include "utils/utils.h"

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
    static const char *short_options = "o:";
    static const struct option long_options[] = {
        { "fuse-opts", required_argument, NULL, 'o' },
        { "delay", no_argument, NULL, 'D' },
        { NULL, 0, NULL, 0, },
    };

    while ((c = getopt_long (argc, argv, short_options,
                             long_options, NULL)) != EOF) {
        switch (c) {
        case 'o':
            g_setenv ("SEADRIVE_FUSE_OPTS", optarg, 1);
            break;
        case 'D':
            msleep(1000);
            break;
        default:
            exit(1);
        }
    }

}


} // anonymous namespace

int main(int argc, char *argv[])
{
    srand(time(NULL));
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    // call glib's init functions
    initGlib();

    // set the domains of settings
    setupSettingDomain();

    // initialize i18n settings
    I18NHelper::getInstance()->init();

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
