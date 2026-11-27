#include "open-local-helper.h"

#include <algorithm>
#include <cstdlib>

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QScopedPointer>
#include <QScopedValueRollback>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <jansson.h>

#include "account-mgr.h"
#include "account.h"
#ifdef Q_OS_MACOS
#include "file-provider-mgr.h"
#endif
#include "rpc/rpc-client.h"
#include "seadrive-gui.h"
#include "utils/file-utils.h"
#include "utils/utils.h"

namespace {

const char *kSeafileProtocolScheme = "seafile";
const char *kSeafileProtocolHostOpenFile = "openfile";
const int kRetryIntervalMs = 1000;
const int kPendingTimeoutMs = 30000;

// URL-only checks. Search deliberately retains its existing path semantics.
bool normalizeRepoPath(const QString& path, QString* normalized, QString* reason = nullptr)
{
    QString ignored_reason;
    if (!reason) {
        reason = &ignored_reason;
    }

    if (path.isEmpty()) {
        *reason = QStringLiteral("the path is empty");
        return false;
    }
    for (const QChar ch : path) {
        if (ch == '\\' || ch == ':' || ch.category() == QChar::Other_Control) {
            *reason = QStringLiteral("the path contains a backslash, colon or control character");
            return false;
        }
    }

    QStringList parts;
    for (const QString& part : path.split('/', Qt::SkipEmptyParts)) {
        if (part == "..") {
            *reason = QStringLiteral("the path contains a parent-directory component");
            return false;
        } else if (part == ".") {
            continue;
        }
#ifdef Q_OS_WIN32
        // Avoid Win32 normalization aliases before checking containment.
        if (part.endsWith('.') || part.endsWith(' ')) {
            *reason = QStringLiteral("a path component ends with a dot or space");
            return false;
        }
#endif
        parts.append(part);
    }
    *normalized = parts.join('/');
    return true;
}

bool parseOpenLocalUrl(const QUrl& url, QString* repo_id, QString* path, QString* reason)
{
    if (!url.isValid()) {
        *reason = QStringLiteral("the URL is invalid: %1").arg(url.errorString());
        return false;
    } else if (url.toEncoded().size() > 16 * 1024) {
        *reason = QStringLiteral("the URL exceeds 16 KiB");
        return false;
    } else if (url.scheme() != kSeafileProtocolScheme || url.host() != kSeafileProtocolHostOpenFile) {
        *reason = QStringLiteral("unsupported scheme or command: \"%1://%2\"")
                      .arg(url.scheme(), url.host());
        return false;
    }

    const QUrlQuery query(url);
    *repo_id = query.queryItemValue("repo_id", QUrl::FullyDecoded);
    const QString input_path = query.queryItemValue("path", QUrl::FullyDecoded);
    if (repo_id->isEmpty()) {
        *reason = QStringLiteral("the repo_id parameter is missing or empty");
        return false;
    } else if (!normalizeRepoPath(input_path, path, reason)) {
        return false;
    }
    *repo_id = repo_id->toLower();
    return true;
}

bool pathIsWithin(const QString& root, const QString& path, bool allow_equal)
{
    const QString parent = QDir::cleanPath(QDir::fromNativeSeparators(root));
    const QString child = QDir::cleanPath(QDir::fromNativeSeparators(path));
    // Be conservative on case-insensitive volumes too. Never fold case and
    // accidentally allow access to a different, case-sensitive directory.
    if (allow_equal && child == parent) {
        return true;
    }
    QString prefix = parent;
    if (!prefix.endsWith('/')) {
        prefix += '/';
    }
    return child.startsWith(prefix);
}

} // namespace


OpenLocalHelper::LocalPathError OpenLocalHelper::resolveLocalPath(
    const Account& account, const QString& repo_id,
    const QString& path_in_repo, LocalPath* result)
{
    *result = LocalPath();
    if (!gui) {
        return LocalPathError::RpcUnavailable;
    }

#ifdef Q_OS_MACOS
    SeafileRpcClient *client = gui->rpcClient(account.domainID());
#else
    SeafileRpcClient *client = gui->rpcClient(EMPTY_DOMAIN_ID);
#endif
    if (!client || !client->isConnected()) {
        return LocalPathError::RpcUnavailable;
    }

    QString repo_name;
    if (!client->getRepoUnameById(repo_id, &repo_name) || repo_name.isEmpty()) {
        return LocalPathError::RepoUnavailable;
    }

    QString root;
#if defined(Q_OS_WIN32)
    root = account.syncRoot;
#elif defined(Q_OS_MACOS)
    root = gui->fileProviderManager()->rootPath(account);
#elif defined(Q_OS_LINUX)
    root = gui->seadriveRoot();
#endif
    if (root.isEmpty() || !QDir::isAbsolutePath(root)) {
        return LocalPathError::RootUnavailable;
    }
#ifdef Q_OS_LINUX
    if (gui->accountManager()->activeAccounts().size() > 1) {
        if (account.displayName.isEmpty()) {
            return LocalPathError::RootUnavailable;
        }
        root = ::pathJoin(root, account.displayName);
    }
#endif
    result->account_root = root;
    result->repo_name = repo_name;
    result->repo_root = ::pathJoin(root, repo_name);
    result->target_path = ::pathJoin(result->repo_root, path_in_repo);
    return LocalPathError::None;
}

OpenLocalHelper::OpenLocalStatus OpenLocalHelper::findLocalAccount(
    const QString& repo_id, Account* account, QString* error)
{
    *account = Account();
    if (!gui || !gui->accountsInitialized()) {
        *error = tr("Waiting for account initialization.");
        return OpenLocalStatus::NotReady;
    }
    auto accounts = gui->accountManager()->activeAccounts();
    if (accounts.isEmpty()) {
        *error = tr("Please log in to SeaDrive before opening this link.");
        return OpenLocalStatus::Failed;
    }

#ifdef Q_OS_MACOS
    // A library may be available in multiple account domains. Use a fixed,
    // case-sensitive order (encoded server URL, then username), independent of
    // UI selection, display names or RPC completion order. Unavailable domains
    // are skipped below, so this selects the first currently usable match.
    std::sort(accounts.begin(), accounts.end(), [](const Account& a, const Account& b) {
        const int server_order = QString::compare(
            a.serverUrl.toString(QUrl::FullyEncoded),
            b.serverUrl.toString(QUrl::FullyEncoded), Qt::CaseSensitive);
        const int username_order = QString::compare(
            a.username, b.username, Qt::CaseSensitive);
        if (server_order != 0) {
            return server_order < 0;
        } else {
            return username_order < 0;
        }
    });

    // Prefer an already available match over waking other account domains.
    for (const Account& candidate : accounts) {
        auto client = gui->rpcClient(candidate.domainID());
        if (!candidate.added || !client || !client->isConnected()) {
            continue;
        }
        QString repo_name;
        if (client->getRepoUnameById(repo_id, &repo_name) && !repo_name.isEmpty()) {
            *account = candidate;
            return OpenLocalStatus::Success;
        }
    }

    // No match is available. Request enumeration once per link for disconnected
    // domains, then let the existing RPC reconnection and request retry proceed.
    if (!reenumerate_requested_) {
        for (const Account& candidate : accounts) {
            auto client = gui->rpcClient(candidate.domainID());
            if (client && client->isConnected()) {
                continue;
            }
            reenumerate_requested_ = true;
            qInfo().noquote() << QStringLiteral("[OpenLocal] Request #%1: Requesting working-set enumeration for domain \"%2\" because its RPC connection is unavailable.")
                .arg(request_id_).arg(candidate.domainID());
            gui->fileProviderManager()->reenumerate(candidate);
        }
    }
    *error = tr("Waiting for the library to become available in a connected account.");
    return OpenLocalStatus::NotReady;
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (!rpc_client || !rpc_client->isConnected()) {
        *error = tr("Waiting for the RPC connection.");
        return OpenLocalStatus::NotReady;
    }

    json_t *ret_obj = nullptr;
    if (!rpc_client->getAccountByRepoId(repo_id, &ret_obj)) {
        *error = tr("Waiting for the local account of this library.");
        return OpenLocalStatus::NotReady;
    }

    *account = gui->accountManager()->getAccountFromJson(ret_obj);
    json_decref(ret_obj);
    if (!account->isValid()) {
        *error = tr("The local account for this library is unavailable.");
        return OpenLocalStatus::Failed;
    }
    return OpenLocalStatus::Success;
#endif
}

OpenLocalHelper::OpenLocalStatus OpenLocalHelper::validateLocalPath(
    const LocalPath& local_path, const QString& path_in_repo,
    QString* path_to_open, QString* error)
{
    path_to_open->clear();
    QString repo_name;
    if (!QDir::isRelativePath(local_path.repo_name) ||
        !normalizeRepoPath(local_path.repo_name, &repo_name) || repo_name.isEmpty()) {
        *error = tr("This link points outside the local library and cannot be opened.");
        return OpenLocalStatus::Failed;
    }
    // Use the normalized name for the URL path, without changing search paths.
    const QString repo_path = QDir::cleanPath(::pathJoin(local_path.account_root, repo_name));
    const QString target_path = QDir::cleanPath(::pathJoin(repo_path, path_in_repo));
    if (!pathIsWithin(local_path.account_root, repo_path, false) ||
        !pathIsWithin(repo_path, target_path, true)) {
        *error = tr("This link points outside the local library and cannot be opened.");
        return OpenLocalStatus::Failed;
    }

    const QFileInfo account_root(local_path.account_root);
    const QFileInfo repo_root(repo_path);
    if (!account_root.isDir()) {
        *error = tr("Waiting for the virtual drive root folder: \"%1\".").arg(local_path.account_root);
        return OpenLocalStatus::NotReady;
    } else if (!repo_root.isDir()) {
        *error = tr("Waiting for the local library folder: \"%1\".").arg(repo_path);
        return OpenLocalStatus::NotReady;
    }
    const QString actual_account_root = account_root.canonicalFilePath();
    const QString actual_repo_root = repo_root.canonicalFilePath();
    if (actual_account_root.isEmpty() || actual_repo_root.isEmpty() ||
        !pathIsWithin(actual_account_root, actual_repo_root, false)) {
        *error = tr("Unable to verify that the path belongs to the local library.");
        return OpenLocalStatus::Failed;
    }
#ifdef Q_OS_LINUX
    const QString actual_seadrive_root = QFileInfo(gui->seadriveRoot()).canonicalFilePath();
    if (actual_seadrive_root.isEmpty() ||
        !pathIsWithin(actual_seadrive_root, actual_account_root, true)) {
        *error = tr("Unable to verify that the path belongs to the local library.");
        return OpenLocalStatus::Failed;
    }
#endif

    const QFileInfo target(target_path);
    if (!target.exists()) {
        *error = tr("The file does not exist or is not yet available locally.");
        return OpenLocalStatus::Failed;
    }
    const QString actual_target = target.canonicalFilePath();
    if (actual_target.isEmpty() || !pathIsWithin(actual_repo_root, actual_target, true)) {
        *error = tr("Unable to verify that the path belongs to the local library.");
        return OpenLocalStatus::Failed;
    }
    if (!target.isFile() && !target.isDir()) {
        *error = tr("The file or folder is not accessible.");
        return OpenLocalStatus::Failed;
    }

    *path_to_open = target_path;
    return OpenLocalStatus::Success;
}

OpenLocalHelper::OpenLocalStatus OpenLocalHelper::openLocalFileByRepo(
    const QString& repo_id, const QString& path_in_repo, QString* error)
{
    // Existing RPC and File Provider root queries are synchronous and may block.
    // NotReady describes returned results; it is not a timeout/cancellation bound.
    Account account;
    auto status = findLocalAccount(repo_id, &account, error);
    if (status != OpenLocalStatus::Success) {
        return status;
    }

    LocalPath local_path;
    const auto path_error = resolveLocalPath(account, repo_id, path_in_repo, &local_path);
    if (path_error != LocalPathError::None) {
        if (path_error == LocalPathError::RpcUnavailable) {
            *error = tr("Waiting for the RPC connection.");
        } else if (path_error == LocalPathError::RepoUnavailable) {
            *error = tr("Waiting for the local library name.");
        } else {
            *error = tr("Waiting for the virtual drive root folder.");
        }
        return OpenLocalStatus::NotReady;
    }

    QString path_to_open;
    status = validateLocalPath(local_path, path_in_repo, &path_to_open, error);
    if (status != OpenLocalStatus::Success) {
        return status;
    }
    if (!openUrl(QUrl::fromLocalFile(path_to_open))) {
        *error = tr("The system could not open the file or folder.");
        return OpenLocalStatus::Failed;
    }
    return OpenLocalStatus::Success;
}

OpenLocalHelper* OpenLocalHelper::singleton_ = NULL;

OpenLocalHelper::OpenLocalHelper()
{
    retry_timer_.setInterval(kRetryIntervalMs);
    connect(&retry_timer_, &QTimer::timeout,
            this, &OpenLocalHelper::checkPendingOpenLocalRequest);

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

bool OpenLocalHelper::openLocalFile(const QUrl& url)
{
    QString repo_id;
    QString path;
    QString reason;
    if (!parseOpenLocalUrl(url, &repo_id, &path, &reason)) {
        qWarning().noquote() << QStringLiteral("[OpenLocal] Rejected the link: %1. URL: %2")
            .arg(reason, QString::fromUtf8(url.toEncoded()));
        QTimer::singleShot(0, this, [this]() {
            if (gui) {
                gui->messageBox(tr("Invalid local file link."));
            }
        });
        return false;
    }

    if (!pending_repo_id_.isEmpty()) {
        qInfo().noquote() << QStringLiteral("[OpenLocal] Request #%1: Replaced by request #%2 after %3 ms.")
            .arg(request_id_).arg(request_id_ + 1).arg(pending_elapsed_.elapsed());
    }
    pending_repo_id_ = repo_id;
    pending_path_ = path;
    ++request_id_;
    processing_started_ = false;
#ifdef Q_OS_MACOS
    reenumerate_requested_ = false;
#endif
    last_wait_reason_.clear();
    pending_elapsed_.start();
    retry_timer_.start();
    QTimer::singleShot(0, this, &OpenLocalHelper::checkPendingOpenLocalRequest);
    // This slot reports acceptance of a valid link, not successful opening.
    return true;
}

void OpenLocalHelper::handleOpenLocalFromCommandLine(const QString &url)
{
    bool connected;
    bool sent = false;
    {
        QScopedPointer<SeaDriveRpcServer::Client> client(SeaDriveRpcServer::getClient());
        connected = client->connect();
        if (connected) {
            sent = client->sendOpenSeafileUrlCommand(QUrl(url));
        }
    }
    // Release the temporary connection before exiting or displaying a dialog.
    if (connected && sent) {
        exit(EXIT_SUCCESS);
    } else if (connected) {
        qWarning("[OpenLocalHelper] failed to forward local file link");
        QMessageBox::warning(nullptr, getBrand(),
                             tr("Unable to send this link to the running client. Please try again later."));
        exit(EXIT_FAILURE);
    }
    openLocalFile(QUrl(url));
}

void OpenLocalHelper::checkPendingOpenLocalRequest()
{
    if (pending_repo_id_.isEmpty() || processing_ || !gui) {
        return;
    }
    QScopedValueRollback<bool> processing(processing_, true);
    // Account initialization happens after the main log handler is installed.
    // Do not consume this one-time message during a cold-start callback.
    if (!processing_started_ && gui->accountsInitialized()) {
        processing_started_ = true;
        qInfo().noquote() << QStringLiteral("[OpenLocal] Request #%1: Processing library \"%2\", path \"%3\".")
            .arg(request_id_).arg(pending_repo_id_, pending_path_);
    }
    if (pending_elapsed_.elapsed() >= kPendingTimeoutMs) {
        QString reason;
        if (last_wait_reason_.isEmpty()) {
            reason = QStringLiteral("Processing did not start before the deadline.");
        } else {
            reason = QStringLiteral("Last waiting state: %1").arg(last_wait_reason_);
        }
        qWarning().noquote() << QStringLiteral("[OpenLocal] Request #%1: Timed out after %2 ms. %3")
            .arg(request_id_).arg(pending_elapsed_.elapsed()).arg(reason);
        clearPendingRequest();
        gui->messageBox(tr("The service or local library is not ready. Please try again later."));
        return;
    }

    // Keep copies: nested events may submit a replacement during synchronous calls.
    const quint64 request_id = request_id_;
    const QString repo_id = pending_repo_id_;
    const QString path = pending_path_;
    QString error;
    const auto status = openLocalFileByRepo(repo_id, path, &error);
    if (request_id != request_id_) {
        return;
    }
    if (status == OpenLocalStatus::NotReady) {
        if (error != last_wait_reason_) {
            last_wait_reason_ = error;
            qInfo().noquote() << QStringLiteral("[OpenLocal] Request #%1: %2")
                .arg(request_id).arg(error);
        }
        return;
    }

    // Clear terminal requests before showing an error dialog (opening, if any,
    // has already returned).
    QString message;
    if (status == OpenLocalStatus::Success) {
        message = QStringLiteral("Submitted the target to the system for opening.");
    } else {
        message = QStringLiteral("Failed to open the target: %1").arg(error);
    }
    const QString line = QStringLiteral("[OpenLocal] Request #%1: %2 Elapsed time: %3 ms.")
        .arg(request_id).arg(message).arg(pending_elapsed_.elapsed());
    if (status == OpenLocalStatus::Failed) {
        qWarning().noquote() << line;
    } else {
        qInfo().noquote() << line;
    }
    clearPendingRequest();
    if (status == OpenLocalStatus::Failed) {
        gui->messageBox(error);
    }
}

void OpenLocalHelper::clearPendingRequest()
{
    pending_repo_id_.clear();
    pending_path_.clear();
    processing_started_ = false;
#ifdef Q_OS_MACOS
    reenumerate_requested_ = false;
#endif
    last_wait_reason_.clear();
    pending_elapsed_.invalidate();
    retry_timer_.stop();
}
