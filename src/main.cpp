#include <getopt.h>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include <glib-object.h>
#include <cstdio>

#include "seadrive-gui.h"

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

} // anonymous namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // call glib's init functions
    initGlib();

    // set the domains of settings
    setupSettingDomain();

    // start applet
    SeadriveGui mGui;
    gui = &mGui;
    QTimer::singleShot(0, gui, SLOT(start()));

    // start qt eventloop
    int ret = app.exec();

    qWarning("app event loop exited with %d\n", ret);

    return ret;
}
