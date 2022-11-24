#include <searpc.h>
#include <searpc-client.h>
#include <searpc-server.h>
#include <searpc-named-pipe-transport.h>

#include "searpc-signature.h"
#include "searpc-marshal.h"

#include <QCoreApplication>

#include "utils/utils.h"
#include "seadrive-gui.h"
#include "settings-mgr.h"
#include "utils/file-utils.h"
#include "rpc-server.h"
#include "open-local-helper.h"

#if defined(Q_OS_WIN32)
#include "utils/utils-win.h"
#endif


class SettingsManager;
namespace {

#if defined(Q_OS_WIN32)
const char *kSeaDriveSockName = "\\\\.\\pipe\\seadrive_client_";
#else
const char *kSeaDriveSockName = "seadrive_client.sock";
#endif
const char *kSeaDriveRpcService = "seadrive-client-rpcserver";

QString getAppletRpcPipePath()
{
#if defined(Q_OS_WIN32)
    return utils::win::getLocalPipeName(kSeaDriveSockName).c_str();
#else
    QString current_cache_dir;
    if (!gui->settingsManager()->getCacheDir(&current_cache_dir)) {
        current_cache_dir = gui->seadriveDataDir();
    }
    if (QDir::isAbsolutePath(current_cache_dir)) {
        current_cache_dir = QDir::home().relativeFilePath(current_cache_dir);
    }
    QString socket_path = pathJoin(current_cache_dir,kSeaDriveSockName);
    return socket_path;
#endif
}

int
handle_exit_command (GError **error)
{
    qWarning("[rpc server] Got a quit command. Quit now.");
    RpcServerProxy::instance()->proxyExitCommand();
    return 0;
}

int
handle_open_seafile_url_command (const char *url, GError **error)
{
    qWarning("[rpc server] opening seafile url %s", url);
    RpcServerProxy::instance()->proxyOpenSeafileUrlCommand(QUrl::fromEncoded(url));
    return 0;
 }

 void register_rpc_service ()
{
    searpc_server_init ((RegisterMarshalFunc)register_marshals);
    searpc_create_service (kSeaDriveRpcService);
    searpc_server_register_function (kSeaDriveRpcService,
                                     (void *)handle_exit_command,
                                     "exit",
                                     searpc_signature_int__void());
    searpc_server_register_function (kSeaDriveRpcService,
                                     (void *)handle_open_seafile_url_command,
                                     "open_seafile_url",
                                     searpc_signature_int__string());
}

 SearpcClient *createSearpcClientWithPipeTransport(const char *rpc_service)
{
    SearpcNamedPipeClient *pipe_client;
    pipe_client = searpc_create_named_pipe_client(toCStr(getAppletRpcPipePath()));
    int ret = searpc_named_pipe_client_connect(pipe_client);
    SearpcClient *c = searpc_client_with_named_pipe_transport(pipe_client, rpc_service);
    if (ret < 0) {
        searpc_free_client_with_pipe_transport(c);
        return nullptr;
    }
    return c;
}

class AppletRpcClient : public SeaDriveRpcServer::Client {
public:
    bool connect() {
        seadrive_rpc_client_ = createSearpcClientWithPipeTransport(kSeaDriveRpcService);
        if (!seadrive_rpc_client_) {
            return false;
        }
        return true;
    }

    bool sendExitCommand() {
        GError *error = NULL;
        int ret = searpc_client_call__int (
            seadrive_rpc_client_,
            "exit",
            &error, 0);
        if (error) {
            g_error_free(error);
            return false;
        }
        if (ret != 0) {
            return false;
        }
        return true;
    }

    bool sendOpenSeafileUrlCommand(const QUrl& url) {
        GError *error = NULL;
        int ret = searpc_client_call__int(
                seadrive_rpc_client_,
                "open_seafile_url",
                &error, 1, "string", url.toEncoded().data());
        if (error) {
            g_error_free(error);
            return false;
        }
        if (ret != 0) {
            return false;
        }
        return true;
    }

private:
    SearpcClient *seadrive_rpc_client_;

 };

 } // namespace

 struct SeaDriveRpcServerPriv {
    SearpcNamedPipeServer *pipe_server;
};

SINGLETON_IMPL(SeaDriveRpcServer)

SeaDriveRpcServer::SeaDriveRpcServer()
: priv_(new SeaDriveRpcServerPriv)
{
    priv_->pipe_server = searpc_create_named_pipe_server(toCStr(getAppletRpcPipePath()));

    RpcServerProxy *proxy = RpcServerProxy::instance();
    connect(proxy, SIGNAL(exitCommand()), this, SLOT(handleExitCommand()));
    connect(proxy,
            SIGNAL(openSeafileUrlCommand(const QUrl &)),
            this,
            SLOT(handleOpenSeafileUrlCommand(const QUrl &)));
}

SeaDriveRpcServer::~SeaDriveRpcServer()
{
}

//start rpc servers to supply rpc communication
void SeaDriveRpcServer::start()
{
    register_rpc_service();
    qWarning("starting applet rpc service");
    if (searpc_named_pipe_server_start(priv_->pipe_server) < 0) {
        qWarning("failed to start rpc service");
    } else {
        qWarning("applet rpc service started");
    }
}

SeaDriveRpcServer::Client* SeaDriveRpcServer::getClient()
{
    return new AppletRpcClient();
}

void SeaDriveRpcServer::handleExitCommand()
{
    qWarning("[Message Listener] Got a quit command. Quit now.");
    QCoreApplication::exit(0);
}

void SeaDriveRpcServer::handleOpenSeafileUrlCommand(const QUrl& url)
{
    OpenLocalHelper::instance()->openLocalFile(url);
}


SINGLETON_IMPL(RpcServerProxy)

RpcServerProxy::RpcServerProxy()
{
}

void RpcServerProxy::proxyExitCommand()
{
    emit exitCommand();
}

void RpcServerProxy::proxyOpenSeafileUrlCommand(const QUrl& url)
{
    emit openSeafileUrlCommand(url);
}
