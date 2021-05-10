#ifndef SEADRIVE_GUI_RPC_SERVER_H
#define SEADRIVE_GUI_RPC_SERVER_H

#include <QObject>

#include "utils/singleton.h"

struct SeaDriveRpcServerPriv;

// This is the rpc server componet to accept commands like --stop.
class SeaDriveRpcServer : public QObject {
    SINGLETON_DEFINE(SeaDriveRpcServer)
    Q_OBJECT
public:
    SeaDriveRpcServer();
    ~SeaDriveRpcServer();

    void start();

    class Client {
    public:
        virtual bool connect() = 0;
        virtual bool sendExitCommand() = 0;
        virtual bool sendOpenSeafileUrlCommand(const QUrl& url) = 0;
    };

    static Client* getClient();

private slots:
    void handleExitCommand();
    void handleOpenSeafileUrlCommand(const QUrl& url);


private:
    SeaDriveRpcServerPriv *priv_;
};

// Helper object to proxy the rpc commands from the rpc server thread
// to the main thread (using signals/slots). We need this because,
// e.g. we can't call QCoreApplication::exit() from non-main thread.
class RpcServerProxy : public QObject {
    SINGLETON_DEFINE(RpcServerProxy)
    Q_OBJECT

public:
    RpcServerProxy();

    void proxyExitCommand();
    void proxyOpenSeafileUrlCommand(const QUrl&);


signals:
    void exitCommand();
    void openSeafileUrlCommand(const QUrl&);
};

#endif
