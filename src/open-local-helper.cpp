#if defined(Q_OS_WIN32)
#include <shellapi.h>
#endif

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#ifdef Q_OS_MACOS
#include "file-provider-mgr.h"
#endif

#include "open-local-helper.h"
#include "utils/utils.h"
#include "utils/file-utils.h"
#include "seadrive-gui.h"
#include "rpc/rpc-client.h"
#include "account-mgr.h"

namespace {

const char *kSeafileProtocolScheme = "seafile";
const char *kSeafileProtocolHostOpenFile = "openfile";


} // namespace


bool openLocalFile(QString& repo_id, QString& path_in_repo)
{
#if defined(Q_OS_WIN32)
    QString repo_name;
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (!rpc_client || !rpc_client->getRepoUnameById(repo_id, &repo_name)) {
        qWarning("failed to get repo uname by %s", toCStr(repo_id));
        return false;
    }

    json_t *ret_obj = nullptr;
    if (!rpc_client->getAccountByRepoId(repo_id, &ret_obj)) {
        qWarning("failed to get account by repo id %s", toCStr(repo_id));
        return false;
    }

    Account account = gui->accountManager()->getAccountFromJson(ret_obj);
    if (account.syncRoot.isEmpty()) {
        qWarning("failed to get account from json");
        return false;
    }

    QString path_to_open = ::pathJoin(account.syncRoot, repo_name, path_in_repo);
    QFileInfo fi(path_to_open);
    if (!fi.exists()) {
        qWarning("the file or directory %s not exists ", toCStr(path_to_open));
        return false;
    }
    return openUrl(QUrl::fromLocalFile(path_to_open));

#else
    qWarning("openLocalFile is not implemented for this platform");
    return false;

#endif
}

OpenLocalHelper* OpenLocalHelper::singleton_ = NULL;

OpenLocalHelper::OpenLocalHelper()
{
    QDesktopServices::setUrlHandler(kSeafileProtocolScheme, this, "openLocalFile");
}

OpenLocalHelper::~OpenLocalHelper()
{
    QDesktopServices::unsetUrlHandler(kSeafileProtocolScheme);
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

// Reject traversal before cleaning the path. A leading slash is repo-relative.
static bool safePath(const QString &path)
{
    if (path.contains(QChar(0)) || path.contains('\\'))
        return false;
    for (const auto &part : path.split('/')) {
        if (part == "..")
            return false;
    }
    return true;
}

bool OpenLocalHelper::dispatch(const QUrl &url, bool *pending)
{
    *pending = false;
    if (!url.isValid() || url.scheme() != kSeafileProtocolScheme ||
        url.host() != kSeafileProtocolHostOpenFile || !url.userInfo().isEmpty() ||
        url.port() != -1 || (!url.path().isEmpty() && url.path() != "/") ||
        url.hasFragment())
        return false;

    // Preserve encoded delimiters until query splitting; decode each value once.
    const QUrlQuery query(url);
    for (const auto &key : {"repo_id", "path", "email"}) {
        if (query.allQueryItemValues(key).size() > 1)
            return false;
    }
    QString repo_id = query.queryItemValue("repo_id", QUrl::FullyDecoded);
    QString path = query.queryItemValue("path", QUrl::FullyDecoded);
    const QString email = query.queryItemValue("email", QUrl::FullyDecoded);
    static const QRegularExpression uuid(
        "\\A[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\z");
    if (!uuid.match(repo_id).hasMatch() || !query.hasQueryItem("path") ||
        !safePath(path) || email.contains(QChar(0)))
        return false;

#ifdef Q_OS_MACOS
    Account selected;
    QString repo_name;
    int matches = 0;
    bool waiting = false;
    for (const auto &account : gui->accountManager()->activeAccounts()) {
        if (!email.isEmpty() && email != account.username && email != account.accountInfo.email)
            continue;
        auto client = gui->rpcClient(account.domainID());
        if (!account.added || !client || !client->isConnected()) {
            waiting = true;
            continue;
        }
        QString name;
        if (client->getRepoUnameById(repo_id, &name)) {
            selected = account;
            repo_name = name;
            ++matches;
        }
    }
    if (matches > 1)
        return false;
    // A disconnected matching account could make an otherwise unique match ambiguous.
    if (waiting) {
        *pending = true;
        return false;
    }
    if (matches != 1 || repo_name.isEmpty() || !safePath(repo_name) ||
        QDir::isAbsolutePath(repo_name))
        return false;

    const QString root = gui->fileProviderManager()->rootPath(selected);
    if (root.isEmpty() || QFileInfo(root).isSymLink())
        return false;
    const QString library = QDir::cleanPath(::pathJoin(root, repo_name));
    const QString target = QDir::cleanPath(::pathJoin(library, path));
    if (target != library && !target.startsWith(library + '/'))
        return false;
    // Do not follow links, including links in the display path. Metadata only:
    // the associated application, not this process, must hydrate placeholders.
    QString current = QDir::cleanPath(root);
    const QString relative = QDir(current).relativeFilePath(target);
    for (const auto &part : relative.split('/', Qt::SkipEmptyParts)) {
        current = ::pathJoin(current, part);
        if (QFileInfo(current).isSymLink())
            return false;
    }
    if (!QFileInfo::exists(target))
        return false;
    return openUrl(QUrl::fromLocalFile(target));
#else
    return ::openLocalFile(repo_id, path);
#endif
}

bool OpenLocalHelper::openLocalFile(const QUrl &url)
{
    bool pending = false;
    const bool opened = dispatch(url, &pending);
    if (pending) {
#ifdef Q_OS_MACOS
        // Preserve the snapshot for older requests: they may already be ready.
        if (pending_urls_.isEmpty())
            last_readiness_ = readiness();
#endif
        pending_urls_.append(url);
    }
    return opened || pending;
}

void OpenLocalHelper::setUrl(const QString &url)
{
    if (!url.isEmpty())
        pending_urls_.append(QUrl(url));
#ifdef Q_OS_MACOS
    last_readiness_.clear();
#endif
}

#ifdef Q_OS_MACOS
QString OpenLocalHelper::readiness() const
{
    QString state = "accounts\n";
    for (const auto &account : gui->accountManager()->activeAccounts()) {
        auto client = gui->rpcClient(account.domainID());
        state += account.domainID() + ':' + QString::number(account.added) +
            QString::number(client && client->isConnected()) + ':' +
            account.username + ':' + account.accountInfo.email + '\n';
    }
    return state;
}
#endif

void OpenLocalHelper::messageBox(const QString& msg)
{
    gui->messageBox(msg);
}

void OpenLocalHelper::handleOpenLocalFromCommandLine(const QString &url)
{
    SeaDriveRpcServer::Client *client = SeaDriveRpcServer::getClient();
    if (client->connect()) {
        // An instance of seadrive applet is running
        client->sendOpenSeafileUrlCommand(QUrl(url));
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
    if (pending_urls_.isEmpty())
        return;
#ifdef Q_OS_MACOS
    const QString state = readiness();
    if (state == last_readiness_)
        return;
    last_readiness_ = state;
#endif
    const auto requests = pending_urls_;
    pending_urls_.clear();
    for (const auto &url : requests) {
        bool pending = false;
        dispatch(url, &pending);
        if (pending)
            pending_urls_.append(url);
    }
}
