#include <sqlite3.h>
#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <algorithm>

#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>

#include "account-mgr.h"
#include "seadrive-gui.h"
#include "utils/utils.h"
#include "api/api-error.h"
#include "api/requests.h"
#if defined(_MSC_VER)
#include "utils/file-utils.h"
#endif
#include "shib/shib-login-dialog.h"
#include "settings-mgr.h"
#include "account-info-service.h"
#include "file-provider-mgr.h"
#include "ui/tray-icon.h"

#if defined (Q_OS_WIN32)
#include "win-sso/auto-logon-dialog.h"
#endif

namespace {
const char *kVersionKeyName = "version";
const char *kFeaturesKeyName = "features";
const char *kCustomBrandKeyName = "custom-brand";
const char *kCustomLogoKeyName = "custom-logo";
const char *kTotalStorage = "storage.total";
const char *kUsedStorage = "storage.used";
const char *kNickname = "name";

bool getShibbolethColumnInfoCallBack(sqlite3_stmt *stmt, void *data)
{
    bool *has_shibboleth_column = static_cast<bool*>(data);
    const char *column_name = (const char *)sqlite3_column_text (stmt, 1);

    if (0 == strcmp("isShibboleth", column_name))
        *has_shibboleth_column = true;

    return true;
}

bool getKerberosColumnInfoCallBack(sqlite3_stmt *stmt, void *data)
{
    bool *has_kerberos_column = static_cast<bool*>(data);
    const char *column_name = (const char *)sqlite3_column_text (stmt, 1);

    if (0 == strcmp("isKerberos", column_name))
        *has_kerberos_column = true;

    return true;
}

bool getAutomaticLoginColumnInfoCallBack(sqlite3_stmt *stmt, void *data)
{
    bool *has_automatic_login_column = static_cast<bool*>(data);
    const char *column_name = (const char *)sqlite3_column_text (stmt, 1);

    if (0 == strcmp("AutomaticLogin", column_name))
        *has_automatic_login_column = true;

    return true;
}

void updateAccountDatabaseForColumnShibbolethUrl(struct sqlite3* db)
{
    bool has_shibboleth_column = false;
    const char* sql = "PRAGMA table_info(Accounts);";
    sqlite_foreach_selected_row (db, sql, getShibbolethColumnInfoCallBack, &has_shibboleth_column);
    sql = "ALTER TABLE Accounts ADD COLUMN isShibboleth INTEGER";
    if (!has_shibboleth_column && sqlite_query_exec (db, sql) < 0)
        qCritical("unable to create isShibboleth column\n");
}

void updateAccountDatabaseForColumnKerberosUrl(struct sqlite3* db)
{
    bool has_kerberos_column = false;
    const char* sql = "PRAGMA table_info(Accounts);";
    sqlite_foreach_selected_row (db, sql, getKerberosColumnInfoCallBack, &has_kerberos_column);
    sql = "ALTER TABLE Accounts ADD COLUMN isKerberos INTEGER";
    if (!has_kerberos_column && sqlite_query_exec (db, sql) < 0)
        qCritical("unable to create isKerberos column\n");
}

void updateAccountDatabaseForColumnAutomaticLogin(struct sqlite3* db)
{
    bool has_automatic_login_column = false;
    const char* sql = "PRAGMA table_info(Accounts);";
    sqlite_foreach_selected_row (db, sql, getAutomaticLoginColumnInfoCallBack, &has_automatic_login_column);
    sql = "ALTER TABLE Accounts ADD COLUMN AutomaticLogin INTEGER default 1";
    if (!has_automatic_login_column && sqlite_query_exec (db, sql) < 0)
        qCritical("unable to create AutomaticLogin column\n");
}

bool compareAccount(const Account& a, const Account& b)
{
    if (!a.isValid()) {
        return false;
    } else if (!b.isValid()) {
        return true;
    } else if (a.lastVisited < b.lastVisited) {
        return false;
    } else if (a.lastVisited > b.lastVisited) {
        return true;
    }

    return true;
}

struct UserData {
    std::vector<Account> *accounts;
    struct sqlite3 *db;
};

#if defined(_MSC_VER)
struct SyncRootInfoData {
    std::vector<SyncRootInfo> *syncrootinfos;
    struct sqlite3 *db;
};
#endif

inline void setServerInfoKeyValue(struct sqlite3 *db, const Account &account, const QString& key, const QString &value)
{
    char *zql = sqlite3_mprintf(
        "REPLACE INTO ServerInfo(url, username, key, value) VALUES (%Q, %Q, %Q, %Q)",
        account.serverUrl.toEncoded().data(), account.username.toUtf8().data(),
        key.toUtf8().data(), value.toUtf8().data());
    sqlite_query_exec(db, zql);
    sqlite3_free(zql);
}

}

AccountManager::AccountManager()
{
    db = NULL;
}

AccountManager::~AccountManager()
{
    if (db)
        sqlite3_close(db);
}

int AccountManager::start()
{
    const char *errmsg;
    const char *sql;

    QString db_path = QDir(gui->seadriveDir()).filePath("accounts.db");
    if (sqlite3_open (toCStr(db_path), &db)) {
        errmsg = sqlite3_errmsg (db);
        qCritical("failed to open account database %s: %s",
                toCStr(db_path), errmsg ? errmsg : "no error given");

        gui->errorAndExit(tr("failed to open account database"));
        return -1;
    }

    // enabling foreign keys, it must be done manually from each connection
    // and this feature is only supported from sqlite 3.6.19
    sql = "PRAGMA foreign_keys=ON;";
    if (sqlite_query_exec (db, sql) < 0) {
        qCritical("sqlite version is too low to support foreign key feature\n");
        sqlite3_close(db);
        db = NULL;
        return -1;
    }

    sql = "CREATE TABLE IF NOT EXISTS Accounts (url VARCHAR(24), "
        "username VARCHAR(15), token VARCHAR(40), lastVisited INTEGER, "
        "PRIMARY KEY(url, username))";
    if (sqlite_query_exec (db, sql) < 0) {
        qCritical("failed to create accounts table\n");
        sqlite3_close(db);
        db = NULL;
        return -1;
    }

    updateAccountDatabaseForColumnShibbolethUrl(db);
    updateAccountDatabaseForColumnAutomaticLogin(db);
    updateAccountDatabaseForColumnKerberosUrl(db);

    // create ServerInfo table
    sql = "CREATE TABLE IF NOT EXISTS ServerInfo ("
        "key TEXT NOT NULL, value TEXT, "
        "url VARCHAR(24), username VARCHAR(15), "
        "PRIMARY KEY(url, username, key), "
        "FOREIGN KEY(url, username) REFERENCES Accounts(url, username) "
        "ON DELETE CASCADE ON UPDATE CASCADE )";
    if (sqlite_query_exec (db, sql) < 0) {
        qCritical("failed to create server_info table\n");
        sqlite3_close(db);
        db = NULL;
        return -1;
    }

#if defined(_MSC_VER)
    sql = "CREATE TABLE IF NOT EXISTS SyncRootInfo ("
        "url VARCHAR(24), "
        "username VARCHAR(15), "
        "syncrootpath TEXT, "
        "PRIMARY KEY(url, username))";
    if (sqlite_query_exec (db, sql) < 0) {
        qCritical("failed to create SyncRootInfo table\n");
        sqlite3_close(db);
        db = NULL;
        return -1;
    }
#endif

    loadAccounts();

#if defined(_MSC_VER)
    loadSyncRootInfo();
#endif

    return 0;
}

bool AccountManager::loadAccountsCB(sqlite3_stmt *stmt, void *data)
{
    UserData *userdata = static_cast<UserData*>(data);
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
    char* zql = sqlite3_mprintf("SELECT key, value FROM ServerInfo WHERE url = %Q AND username = %Q", url, username);
    sqlite_foreach_selected_row (userdata->db, zql, loadServerInfoCB, &account);
    sqlite3_free(zql);

    userdata->accounts->push_back(account);
    return true;
}

bool AccountManager::loadServerInfoCB(sqlite3_stmt *stmt, void *data)
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

#if defined(_MSC_VER)
bool AccountManager::loadSyncRootInfoCB(sqlite3_stmt *stmt, void* data)
{

    SyncRootInfoData *sync_root_info_data = static_cast<SyncRootInfoData* >(data);
    const char *url = (const char *)sqlite3_column_text(stmt, 0);
    const char *username = (const char *)sqlite3_column_text(stmt, 1);
    const char *sync_root_path = (const char *)sqlite3_column_text(stmt, 2);

    SyncRootInfo sync_root_info = SyncRootInfo(url, username, sync_root_path);

    sync_root_info_data->syncrootinfos->push_back(sync_root_info);
    return true;
}

void AccountManager::loadSyncRootInfo()
{
    const char* sql = "SELECT url, username, syncrootpath From SyncRootInfo";
    sync_root_infos_.clear();
    SyncRootInfoData sync_root_info_data;
    sync_root_info_data.syncrootinfos = &sync_root_infos_;
    sync_root_info_data.db = db;
    sqlite_foreach_selected_row(db, sql, loadSyncRootInfoCB, &sync_root_info_data);
}
#endif

const std::vector<Account>& AccountManager::loadAccounts()
{
    const char *sql = "SELECT url, username, token, lastVisited, isShibboleth, AutomaticLogin, isKerberos "
                      "FROM Accounts ORDER BY lastVisited DESC";
    accounts_.clear();
    UserData userdata;
    userdata.accounts = &accounts_;
    userdata.db = db;
    sqlite_foreach_selected_row (db, sql, loadAccountsCB, &userdata);

    std::stable_sort(accounts_.begin(), accounts_.end(), compareAccount);

    qWarning("loaded %d accounts", (int)accounts_.size());

    return accounts_;
}

void AccountManager::enableAccount(const Account& account) {
    Account new_account = account;
    new_account.lastVisited = QDateTime::currentMSecsSinceEpoch();

    size_t i;
    for (i = 0; i < accounts_.size(); i++) {
        if (accounts_[i] == account) {
            accounts_.erase(accounts_.begin() + i);
            break;
        }
    }
    accounts_.insert(accounts_.begin(), new_account);

    char *zql = sqlite3_mprintf(
        "REPLACE INTO Accounts(url, username, token, lastVisited, isShibboleth, AutomaticLogin, isKerberos)"
        "VALUES (%Q, %Q, %Q, %Q, %Q, %Q, %Q) ",
        // url
        new_account.serverUrl.toEncoded().data(),
        // username
        new_account.username.toUtf8().data(),
        // token
        new_account.token.toUtf8().data(),
        // lastVisited
        QString::number(new_account.lastVisited).toUtf8().data(),
        // isShibboleth
        QString::number(new_account.isShibboleth).toUtf8().data(),
        // isAutomaticLogin
        QString::number(new_account.isAutomaticLogin).toUtf8().data(),
        // isKerberos
        QString::number(new_account.isKerberos).toUtf8().data());
    sqlite_query_exec(db, zql);
    sqlite3_free(zql);

    fetchAccountInfoFromServer(account);

    emit accountsChanged();
}

void AccountManager::disableAccount(const Account& account) {
    if (!account.isValid()) {
        return;
    }
    clearAccountToken(account);
}

int AccountManager::removeAccount(const Account& account)
{
    char *zql = sqlite3_mprintf(
        "DELETE FROM Accounts WHERE url = %Q AND username = %Q",
        // url
        account.serverUrl.toEncoded().data(),
        // username
        account.username.toUtf8().data());
    sqlite_query_exec(db, zql);
    sqlite3_free(zql);

    accounts_.erase(
        std::remove(accounts_.begin(), accounts_.end(), account),
        accounts_.end());

    AccountMessage msg;
    msg.type = AccountRemoved;
    msg.account = account;
    messages.enqueue(msg);

    emit accountMQUpdated();

    if (accounts().empty()) {
        gui->trayIcon()->showLoginDialog();
    }

    return 0;
}

void AccountManager::updateAccountLastVisited(const Account& account)
{
    char *zql = sqlite3_mprintf(
        "UPDATE Accounts SET lastVisited = %Q "
        "WHERE url = %Q AND username = %Q",
        // lastVisted
        QString::number(QDateTime::currentMSecsSinceEpoch()).toUtf8().data(),
        // url
        account.serverUrl.toEncoded().data(),
        // username
        account.username.toUtf8().data());
    sqlite_query_exec(db, zql);
    sqlite3_free(zql);
}

#if defined(_MSC_VER)
void AccountManager::updateSyncRootInfo(SyncRootInfo& sync_root_info)
{
    char *zql = sqlite3_mprintf(
            "REPLACE INTO SyncRootInfo(url, username, syncrootpath)"
            "VALUES (%Q, %Q, %Q) ",
            // url
            sync_root_info.getUrl().toUtf8().data(),
            // username
            sync_root_info.getUserName().toUtf8().data(),
            // sync root name
            sync_root_info.syncRootName().toUtf8().data());
    sqlite_query_exec(db, zql);
    sqlite3_free(zql);
}
#endif

bool AccountManager::accountExists(const QUrl& url, const QString& username) const
{
    for (size_t i = 0; i < accounts_.size(); i++) {
        if (accounts_[i].serverUrl == url && accounts_[i].username == username) {
            return true;
        }
    }

    return false;
}

void AccountManager::validateAndUseAccounts() {
    if (accounts().empty()) {
        return;
    }

    if (activeAccounts().empty()) {
        const Account &account = accounts().front();

        if (!account.isAutomaticLogin && account.lastVisited < gui->startupTime()) {
            clearAccountToken(account, true);
        } else {
            reloginAccount(account);
        }

        return;
    }

    auto accounts = activeAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        enableAccount(accounts.at(i));

        // If the account is an old account, the account
        // information has been updated at this time so
        // you can calculate the SyncRoot and then call the switch account.
#if defined(_MSC_VER)
        const Account& current_account = currentAccount();
        sync_root_name_= genSyncRootName(current_account);
        qWarning("generated sync root name is : %s", toCStr(sync_root_name_));
#endif
    }
}

Account AccountManager::getAccountByUrlAndUsername(const QString& url,
                                                   const QString& username) const
{
    Account account = getAccount(url, username);
    if (account.isValid()) {
        return account;
    }

    // Fix the case when the url loses or adds the "/" suffix.
    if (url.endsWith("/")) {
        return getAccount(url.chopped(1), username);
    } else {
        return getAccount(url + "/", username);
    }
}

Account AccountManager::getAccountBySignature(const QString& account_sig) const
{
    for (size_t i = 0; i < accounts_.size(); i++) {
        if (accounts_[i].getSignature() == account_sig) {
            return accounts_[i];
        }
    }

    return Account();
}

void AccountManager::fetchAccountInfoFromServer(const Account& account)
{
    FetchAccountInfoRequest* fetch_account_info_request = new FetchAccountInfoRequest(account);
    connect(fetch_account_info_request, SIGNAL(success(const AccountInfo&)), this,
            SLOT(slotUpdateAccountInfoSucess(const AccountInfo&)));
    connect(fetch_account_info_request, SIGNAL(failed(const ApiError&)), this,
            SLOT(slotUpdateAccountInfoFailed()));
    fetch_account_info_request->send();
}

void AccountManager::updateAccountServerInfo(const Account& account)
{
    ServerInfoRequest *request = new ServerInfoRequest(account);
    connect(request, SIGNAL(success(const Account&, const ServerInfo &)),
            this, SLOT(serverInfoSuccess(const Account&, const ServerInfo &)));
    connect(request, SIGNAL(failed(const ApiError&)),
            this, SLOT(serverInfoFailed(const ApiError&)));
    request->send();
}

const Account AccountManager::updateAccountInfo(const Account& account,
                                                const AccountInfo& info)
{
    setServerInfoKeyValue(db, account, kTotalStorage,
                          QString::number(info.totalStorage));
    setServerInfoKeyValue(db, account, kUsedStorage,
                          QString::number(info.usedStorage));
    setServerInfoKeyValue(db, account, kNickname,
                          info.name);

    for (size_t i = 0; i < accounts_.size(); i++) {
        if (accounts_[i] == account) {
            accounts_[i].accountInfo = info;
            emit accountInfoUpdated(accounts_[i]);
            return accounts_[i];
        }
    }

    return Account();
}


void::AccountManager::slotUpdateAccountInfoSucess(const AccountInfo& info)
{
    FetchAccountInfoRequest* req = (FetchAccountInfoRequest*)(sender());
    const Account account = updateAccountInfo(req->account(), info);
#if defined(Q_OS_MAC)
    if (account.isValid()) {
        gui->fileProviderManager()->registerDomain(account);
    }
#endif
    updateAccountServerInfo(req->account());

    req->deleteLater();
    req = NULL;

    AccountMessage msg;
    msg.type = AccountAdded;
    msg.account = account;
    messages.enqueue(msg);

    emit accountMQUpdated();
}

void AccountManager::slotUpdateAccountInfoFailed()
{
    FetchAccountInfoRequest* req = (FetchAccountInfoRequest*)(sender());
    req->deleteLater();
    req = NULL;
}

void AccountManager::serverInfoSuccess(const Account &account, const ServerInfo &info)
{
    ServerInfoRequest *req = (ServerInfoRequest *)(sender());
    req->deleteLater();

    setServerInfoKeyValue(db, account, kVersionKeyName, info.getVersionString());
    setServerInfoKeyValue(db, account, kFeaturesKeyName, info.getFeatureStrings().join(","));
    setServerInfoKeyValue(db, account, kCustomLogoKeyName, info.customLogo);
    setServerInfoKeyValue(db, account, kCustomBrandKeyName, info.customBrand);

    QUrl url(account.serverUrl);
    url.setPath("/");


#if defined(_MSC_VER)
    const Account& current_account = currentAccount();
    sync_root_name_= genSyncRootName(current_account);
    qWarning("generated sync root name is : %s", toCStr(sync_root_name_));
#endif

    bool changed = account.serverInfo != info;
    if (!changed)
        return;


    for (size_t i = 0; i < accounts_.size(); i++) {
        if (accounts_[i] == account) {
            accounts_[i].serverInfo = info;
            if (i == 0)
                emit accountsChanged();
            break;
        }
    }

}

void AccountManager::serverInfoFailed(const ApiError &error)
{
    ServerInfoRequest *req = (ServerInfoRequest *)(sender());
    req->deleteLater();

    qWarning("update server info failed %s\n", error.toString().toUtf8().data());
}

void AccountManager::clearAccountToken(const Account& account,
                                       bool force_relogin)
{
    for (size_t i = 0; i < accounts_.size(); i++) {
        if (accounts_[i] == account) {
            accounts_[i].token = "";
            break;
        }
    }

    char *zql = sqlite3_mprintf(
        "UPDATE Accounts "
        "SET token = NULL "
        "WHERE url = %Q "
        "  AND username = %Q",
        // url
        account.serverUrl.toEncoded().data(),
        // username
        account.username.toUtf8().data()
        );
    sqlite_query_exec(db, zql);
    sqlite3_free(zql);

    if (force_relogin) {
        reloginAccount(account);
    } else {
        emit accountsChanged();
    }
}

#if defined(_MSC_VER)
const QString AccountManager::getOldSyncRootDir(const Account& account)
{

    QString username = account.username;
    QString addr = account.serverUrl.host();

    QString sync_dir = QString("%1_%2").arg(addr).arg(username);
    QByteArray sync_dir_md5 = QCryptographicHash::hash(sync_dir.toUtf8(),
                                                    QCryptographicHash::Md5).toHex();

    QString mid_sync_dir_md5 = sync_dir_md5.mid(0, 8);

    QDir dir(gui->seadriveRoot());
    if (dir.exists(mid_sync_dir_md5)) {
        return mid_sync_dir_md5;
    }

    return "";
}

const QString AccountManager::genSyncRootName(const Account& account)
{
    QString url = account.serverUrl.toString();
    QString nickname = account.accountInfo.name;
    QString email = account.username;
    QString seadrive_root = gui->seadriveRoot();
    QString sync_root_path, sync_root_name;

    qDebug("[%s] url is %s, nickname is %s, email is %s", __func__,
                        toCStr(url), toCStr(nickname), toCStr(email));

    QString old_sync_dir = getOldSyncRootDir(account);
    if (!old_sync_dir.isEmpty()) {
        return old_sync_dir;
    }

    foreach (SyncRootInfo sync_root_info, sync_root_infos_)
    {
        if (url == sync_root_info.getUrl() && email == sync_root_info.getUserName()) {
            QString sync_root_name = sync_root_info.syncRootName();
            if (!sync_root_name.isEmpty()) {
                qWarning("use exist syncroot name %s", toCStr(sync_root_name));
                return sync_root_name;
            }
        }
    }

    if (!nickname.isEmpty()) {
        sync_root_name = toCStr(nickname);
    } else {
        int pos = email.indexOf("@");
        sync_root_name = email.left(pos);
    }

    // Delete windows reserved characters
    QRegularExpression rx("[<>:\"/\\\\|?*]");
    sync_root_name = sync_root_name.remove(rx);

    if (sync_root_name.size() > 8) {
        sync_root_name = sync_root_name.left(8);
    }

    // Windows 10 folders name cannot end with a dot
    while(sync_root_name.endsWith(".") || sync_root_name.endsWith(" ")) {
       sync_root_name.resize(sync_root_name.size()-1);
    }

    if (sync_root_name.isEmpty()) {
        qWarning("invalid sync root name");
        return "";
    }

    QString new_sync_root_name = sync_root_name;


    // Get all sync root names except current account.
    QVector<QString> other_account_sync_root_names;
    foreach (SyncRootInfo sync_root_info, sync_root_infos_)
    {
        if (sync_root_info.getUrl() != url || sync_root_info.getUserName() != sync_root_name) {
            QString sync_root_name = sync_root_info.syncRootName();
            other_account_sync_root_names.push_back(sync_root_name);
        }
    }

    QDir dir(seadrive_root);
    int i = 1;

    while(dir.exists(new_sync_root_name)) {
        new_sync_root_name = QString("%1_%2").arg(sync_root_name).arg(i);
        i++;
    }

    while (other_account_sync_root_names.contains(new_sync_root_name)) {
        new_sync_root_name = QString("%1_%2").arg(new_sync_root_name).arg(i);
        i++;
    }

    sync_root_name = new_sync_root_name;

    SyncRootInfo sync_root_info(url, email, new_sync_root_name);
    updateSyncRootInfo(sync_root_info);
    sync_root_infos_.push_back(sync_root_info);

    qDebug("[%s] This a new accout gen a new sync root name is %s", __func__, toCStr(new_sync_root_name));
    return new_sync_root_name;
}

#endif

void AccountManager::reloginAccount(const Account &account)
{
    if (account.isShibboleth) {
        ShibLoginDialog shib_dialog(account.serverUrl, gui->settingsManager()->getComputerName());
        shib_dialog.exec();
        return;
    }

#if defined(Q_OS_WIN32)
    if (account.isKerberos) {
        AutoLogonDialog dialog;
        dialog.exec();
        return;
    }
#endif

    gui->trayIcon()->showLoginDialog(account);
}

const std::vector<Account>& AccountManager::accounts() const
{
    return accounts_;
}

const QVector<Account> AccountManager::activeAccounts() const {
    QVector<Account> accounts;
    for (size_t i = 0; i < accounts_.size(); i++) {
        if (!accounts_.at(i).token.isEmpty()) {
            accounts.push_back(accounts_.at(i));
        }
    }
    return accounts;
}

Account AccountManager::getAccount(const QString& url, const QString& username) const {
    for (size_t i = 0; i < accounts_.size(); i++) {
        if (accounts_.at(i).serverUrl.toString() == url &&
            accounts_.at(i).username == username) {
            return accounts_[i];
        }
    }
    return Account();
}

#if defined(_MSC_VER)
const std::vector<SyncRootInfo>& AccountManager::getSyncRootInfos() const
{
    return sync_root_infos_;
}
#endif

bool AccountManager::hasAccount() const {
    return !accounts_.empty();
}

const Account AccountManager::currentAccount() const {
    return hasAccount() ? accounts_[0] : Account();
}
