#include "seadrive-gui.h"

#include <QtGlobal>

#if defined(Q_OS_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <memory>
#include <fts.h>
#include <errno.h>
#include <unistd.h>
#endif

#include <glib.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "utils/utils.h"
#include "settings-mgr.h"
#include "utils/file-utils.h"
#include "ui/uninstall-helper-dialog.h"
#include "rpc/rpc-server.h"

#if defined(Q_OS_WIN32)
#include "utils/registry.h"
#endif

#include "uninstall-helpers.h"

#if defined(_MSC_VER)
#include <sqlite3.h>
#include "seadrive-gui.h"
#include "account-mgr.h"
#include "utils/utils-win.h"
#endif

#if defined(_MSC_VER)
struct UserData {
    std::vector<Account> *accounts;
    struct sqlite3 *db;
};

const char *kVersionKeyName = "version";
const char *kFeaturesKeyName = "features";
const char *kCustomBrandKeyName = "custom-brand";
const char *kCustomLogoKeyName = "custom-logo";
const char *kTotalStorage = "storage.total";
const char *kUsedStorage = "storage.used";
const char *kNickname = "name";
#endif //_MSC_VER


namespace {

#if defined(Q_OS_WIN32)
const char *kPreconfigureKeepConfigWhenUninstall = "PreconfigureKeepConfigWhenUninstall";
#endif

#if !defined(Q_OS_WIN32)
int posix_rmdir(const QString &root)
{
    if (!QFileInfo(root).exists()) {
        qWarning("dir %s doesn't exists", toCStr(root));
        return -1;
    }

    std::unique_ptr<char[]> root_ptr(strdup(toCStr(root)));

    char *paths[] = {root_ptr.get(), NULL};

    // Using `FTS_PHYSICAL` here because we need `FTSENT` for the
    // symbolic link in the directory and not the target it links to.
    FTS *tree = fts_open(paths, (FTS_NOCHDIR | FTS_PHYSICAL), NULL);
    if (tree == NULL) {
        qWarning("failed to fts_open: %s", strerror(errno));
        return -1;
    }

    FTSENT *node;
    while ((node = fts_read(tree)) != NULL) {
        // printf ("%s: fts_info = %d\n", node->fts_path, (int)(node->fts_info));
        switch (node->fts_info) {
            case FTS_DP:
                // qWarning("removing directory %s", node->fts_path);
                if (rmdir(node->fts_path) < 0 && errno != ENOENT) {
                    qWarning("failed to remove dir %s", node->fts_path);
                }
                break;
            // `FTS_DEFAULT` would include any file type which is not
            // explicitly described by any of the other `fts_info` values.
            case FTS_DEFAULT:
            case FTS_F:
            case FTS_SL:
            // `FTS_SLNONE` should never be the case as we don't set
            // `FTS_COMFOLLOW` or `FTS_LOGICAL`. Adding here for completion.
            case FTS_SLNONE:
                // qWarning("removing file %s", node->fts_path);
                if (unlink(node->fts_path) < 0 && errno != ENOENT) {
                    qWarning("failed to remove file %s", node->fts_path);
                }
                break;
            default:
                break;
        }
    }

    if (errno != 0) {
        fts_close(tree);
        return -1;
    }

    if (fts_close(tree) < 0) {
        return -1;
    }
    return 0;
}
#endif

} // namespace


int delete_dir_recursively(const QString& path_in)
{
    qWarning ("removing folder %s\n", toCStr(path_in));
    if (path_in.length() <= 3) {
        // avoid errornous delete drives like C:/ D:/ E:/
        return -1;
    }

    QDir dir (path_in);
    bool ok = dir.removeRecursively();
    if (!ok) {
        qWarning("failed to removeRecursively remove directory %s",toCStr(path_in));
        return -1 ;
    }
    return 0;

}

void do_stop_app()
{
    SeaDriveRpcServer::Client *client = SeaDriveRpcServer::getClient();
    if (!client->connect()) {
        printf ("failed to connect to applet rpc server\n");
        return;
    }
    if (client->sendExitCommand()) {
        printf ("exit command: success\n");
    } else {
        printf ("exit command: failed\n");
    }
}

#if defined(Q_OS_WIN32)
int hasPreconfigureKeepConfigWhenUninstall()
{
    return RegElement::getPreconfigureIntValue(kPreconfigureKeepConfigWhenUninstall);
}
#endif

void do_remove_user_data()
{
    do_stop_app();
    set_seafile_auto_start(false);

#if defined(Q_OS_WIN32)
    if (hasPreconfigureKeepConfigWhenUninstall()) {
        return;
    }
#endif

    SettingsManager::removeAllSettings();

    UninstallHelperDialog *dialog = new UninstallHelperDialog;

    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    qApp->exec();
}

#if defined(_MSC_VER)
static bool loadServerInfoCB(sqlite3_stmt *stmt, void *data)
{
    Account *account = static_cast<Account*>(data);
    const char *key = (const char *)sqlite3_column_text (stmt, 0);
    const char *value = (const char *)sqlite3_column_text (stmt, 1);
    QString key_string = key;
    QString value_string = value;
    if (key_string == kVersionKeyName) {
        account->serverInfo.parseVersionFromString(value_string);
    } else if (key_string == kFeaturesKeyName) {
        account->serverInfo.parseFeatureFromStrings(value_string.split(","));
    } else if (key_string == kCustomBrandKeyName) {
        account->serverInfo.customBrand = value_string;
    } else if (key_string == kCustomLogoKeyName) {
        account->serverInfo.customLogo = value_string;
    } else if (key_string == kTotalStorage) {
        account->accountInfo.totalStorage = value_string.toLongLong();
    } else if (key_string == kUsedStorage) {
        account->accountInfo.usedStorage = value_string.toLongLong();
    } else if (key_string == kNickname) {
        account->accountInfo.name = value_string;
    }
    return true;
}

static bool loadAccountsCB(sqlite3_stmt *stmt, void *data)
{
    UserData *userdata = static_cast<UserData *>(data);
    const char *url = (const char *)sqlite3_column_text (stmt, 0);
    const char *username = (const char *)sqlite3_column_text (stmt, 1);
    const char *token = (const char *)sqlite3_column_text (stmt, 2);
    qint64 atime = (qint64)sqlite3_column_int64 (stmt, 3);
    int isShibboleth = sqlite3_column_int (stmt, 4);
    int isAutomaticLogin = sqlite3_column_int (stmt, 5);
    int isKerberos = sqlite3_column_int (stmt, 6);

    if (!token) {
        token = "";
    }

    Account account = Account(QUrl(QString(url)), QString(username),
        QString(token), atime, isShibboleth != 0,
        isAutomaticLogin != 0, isKerberos != 0);
    char *zql = sqlite3_mprintf("SELECT key, value FROM ServerInfo WHERE url = %Q AND username = %Q", url, username);
    sqlite_foreach_selected_row (userdata->db, zql, loadServerInfoCB, &account);
    sqlite3_free(zql);

    userdata->accounts->push_back(account);
    return true;
}

static void read_account_info_from_db(std::vector<Account>* ptr_accounts)
{
    const char *errmsg;
    struct sqlite3 *db;

    QString db_path = QDir(seadriveDir()).filePath("accounts.db");
    if (sqlite3_open (toCStr(db_path), &db)) {
        errmsg = sqlite3_errmsg (db);
        qCritical("failed to open account database %s: %s",
            toCStr(db_path), errmsg ? errmsg : "no error given");

        return;
    }

    const char *sql = "SELECT url, username, token, lastVisited, isShibboleth, AutomaticLogin, isKerberos "
        "FROM Accounts ORDER BY lastVisited DESC";
    ptr_accounts->clear();
    UserData userdata;
    userdata.accounts = ptr_accounts;
    userdata.db = db;
    sqlite_foreach_selected_row (db, sql, loadAccountsCB, &userdata);

    sqlite3_close(db);
    return;

}

void do_seadrive_unregister_sync_root()
{

    std::vector<Account> accounts;
    read_account_info_from_db(&accounts);

    QString serverAddr, user_name;

    foreach(const Account& account, accounts) {
        serverAddr = account.serverUrl.toString();
        user_name = account.username;

        if (serverAddr.endsWith("/")) {
            serverAddr = serverAddr.left(serverAddr.size() - 1);
        }

        utils::win::seadrive_unregister_sync_root(serverAddr.toStdWString().data(), user_name.toStdWString().data());
    }

}
#endif // _MSC_VER
