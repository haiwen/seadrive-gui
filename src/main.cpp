#include <getopt.h>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include <glib-object.h>
#include <cstdio>

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

    // start qt eventloop
    return app.exec();
}
