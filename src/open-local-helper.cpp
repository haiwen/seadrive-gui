#if defined(Q_OS_WIN32)
#include <shellapi.h>
#endif

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <searpc-client.h>

#include <searpc.h>

#include "account.h"
#include "open-local-helper.h"
#include "utils/utils.h"
#include "utils/file-utils.h"
#include "seadrive-gui.h"
#include "rpc/rpc-client.h"


namespace {

const char *kSeafileProtocolScheme = "seafile";
const char *kSeafileProtocolHostOpenFile = "openfile";


} // namespace


void openLocalFile(QString& repo_id, QString& path_in_repo)
{
    QString repo_name;
    if (!gui->rpcClient()->getRepoUnameById(repo_id, &repo_name)) {
        qWarning("failed to get repo uname by %s", toCStr(repo_id));
        return;
    }

    json_t *ret_obj = nullptr;
    if (!gui->rpcClient()->getAccountByRepoId(repo_id, &ret_obj)) {
        qWarning("failed to get account by repo id %s", toCStr(repo_id));
        return;
    }

    Account account = gui->accountManager()->getAccountFromJson(ret_obj);
    if (account.syncRoot.isEmpty()) {
        qWarning("failed to get account from json");
        return;
    }

    QString path_to_open = ::pathJoin(account.syncRoot, repo_name, path_in_repo);
    QFileInfo fi(path_to_open);
    if (!fi.exists()) {
        qWarning("the file or directory %s not exists ", toCStr(path_to_open));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path_to_open));
    return;
}

OpenLocalHelper* OpenLocalHelper::singleton_ = NULL;

OpenLocalHelper::OpenLocalHelper()
{
    url_ = NULL;

    QDesktopServices::setUrlHandler(kSeafileProtocolScheme, this, SLOT(openLocalFile(const QUrl&)));
}

OpenLocalHelper*
OpenLocalHelper::instance()
{
    if (singleton_ == NULL) {
        static OpenLocalHelper instance;
        singleton_ = &instance;
    }

    return singleton_;
}

bool OpenLocalHelper::openLocalFile(const QUrl &url)
{
    if (url.scheme() != kSeafileProtocolScheme) {
        qWarning("[OpenLocalHelper] unknown scheme %s\n", url.scheme().toUtf8().data());
        return false;
    }

    if (url.host() != kSeafileProtocolHostOpenFile) {
        qWarning("[OpenLocalHelper] unknown command %s\n", url.host().toUtf8().data());
        return false;
    }

    QUrlQuery url_query = QUrlQuery(url.query());
    QString repo_id = url_query.queryItemValue("repo_id", QUrl::FullyDecoded);
    QString email = url_query.queryItemValue("email", QUrl::FullyDecoded);
    QString path = url_query.queryItemValue("path", QUrl::FullyDecoded);

    if (repo_id.size() < 36) {
        qWarning("[OpenLocalHelper] invalid repo_id %s\n", repo_id.toUtf8().data());
        return false;
    }

    qDebug("[OpenLocalHelper] open local file: repo %s, path %s\n",
           repo_id.toUtf8().data(), path.toUtf8().data());

    qWarning("get file repo id is %s, eamil is %s, path is %s\n", toCStr(repo_id), toCStr(email), toCStr(path));
    ::openLocalFile(repo_id, path);

    return true;
}

void OpenLocalHelper::messageBox(const QString& msg)
{
    gui->messageBox(msg);
}

void OpenLocalHelper::handleOpenLocalFromCommandLine(const char *url)
{
    SeaDriveRpcServer::Client *client = SeaDriveRpcServer::getClient();
    if (client->connect()) {
        // An instance of seadrive applet is running
        client->sendOpenSeafileUrlCommand(QUrl::fromEncoded(url));
        exit(0);
    } else {
        // No instance of seadrive client running, we just record the url and
        // let the applet start. The local file will be opened when the applet
        // is ready.
        setUrl(url);
    }
}

void OpenLocalHelper::checkPendingOpenLocalRequest()
{
    if (!url_.isEmpty()) {
        openLocalFile(QUrl::fromEncoded(url_));
        setUrl(NULL);
    }
}
