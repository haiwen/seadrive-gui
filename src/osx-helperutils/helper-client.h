#ifndef SEAFILE_HELPER_CLIENT_H
#define SEAFILE_HELPER_CLIENT_H

#include <QObject>

class HelperClient : public QObject
{
    Q_OBJECT
public:
    HelperClient();
    bool getVersion(QString *version);
    bool installKext(bool *finished, bool *ok);

signals:
    void versionDone();

private:
    void ensureConnected();
    void xpcConnect();
};

#endif // SEAFILE_HELPER_CLIENT_H
