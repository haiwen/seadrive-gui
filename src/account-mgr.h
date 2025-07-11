#ifndef _SEAF_ACCOUNT_MGR_H
#define _SEAF_ACCOUNT_MGR_H

#include <vector>
#include <jansson.h>

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QQueue>

#include "account.h"

struct sqlite3;
struct sqlite3_stmt;
class ApiError;
class SeafileRpcClient;

#ifdef Q_OS_WIN32
class SyncRootInfo {
public:

    SyncRootInfo(QString url, QString username, QString sync_root_path) {
        url_ = url;
        username_ = username;
        sync_root_path_ = sync_root_path;
    }

    QString getUrl() { return url_; }
    QString getUserName() { return username_; }
    QString syncRootName() { return sync_root_path_; }

private:
    QString url_;
    QString username_;
    QString sync_root_path_;

};
#endif

class AccountManager : public QObject {
    Q_OBJECT

public:
    AccountManager();
    ~AccountManager();

    int start();

    // Load the accounts from local db when client starts.
    void loadAccounts();

    /**
     * Account operations
     */

    // Save the account state after being logged in.
    void enableAccount(const Account& account);
    // Save the account state after being logged out.
    void disableAccount(const Account& account);

    // Remove the account. Used when user removes an account from the
    // account menu.
    int removeAccount(const Account& account);

    // Resync the account. Used when user resync an account from the
    // account menu.
    int resyncAccount(const Account& account);

    // Use all valid accounts, or re-login if no valid account.
    void validateAndUseAccounts();

    // Update AccountInfo (e.g. nick name, quota etc.) for the given
    // account, and return the updated account.
    const Account updateAccountInfo(const Account& account, const AccountInfo& info);

    /**
     * Accessors
     */
    const QVector<Account> allAccounts() const;
    const QVector<Account> activeAccounts() const;
    bool hasAccount() const;
    bool accountExists(const QUrl& url, const QString& username) const;

    Account getAccountByUrlAndUsername(const QString& url,
                                       const QString& username) const;

    Account getAccountBySignature(const QString& account_sig) const;

    Account getAccountFromJson(json_t *ret_obj) const;

    Account getAccountByDomainID (const QString& domain_id) const;


    void clearAccountToken(const Account& account,
                           bool force_relogin=false);

    void setAccountAdded(Account& account, bool added);

    void setAccountNotifiedStartExtension(Account& account, bool notified_start_extension);

    void addAccountConnectDaemonRetry(Account& account);

#ifdef Q_OS_WIN32
    QString getPreviousSyncRootName(const Account& account);
    const QString genSyncRootName(const Account& account);
    void setSyncRootName(const Account& account, const QString& custom_name);
#endif

public slots:
    void reloginAccount(const Account &account);

signals:
    /**
     * Account added/removed/switched.
     */
    void accountInfoUpdated(const Account& account);

private slots:
    void slotUpdateAccountInfoSucess(const AccountInfo& info);
    void slotUpdateAccountInfoFailed();
    void serverInfoSuccess(const ServerInfo &info);
    void serverInfoFailed(const ApiError&);

private:
    Q_DISABLE_COPY(AccountManager)

    void fetchAccountInfoFromServer(const Account& account);
    void updateAccountServerInfo(const Account& account);
    static bool loadAccountsCB(struct sqlite3_stmt *stmt, void *data);
    static bool loadServerInfoCB(struct sqlite3_stmt *stmt, void *data);

    void updateAccountLastVisited(const Account& account);
    Account getAccount(const QString& url, const QString& username) const;
    void addAccountToDaemon(const Account& account);

    struct sqlite3 *db;

    // accounts_ will be accessed by multiple threads, thus it should be protected by the accounts_mutex_.
    // For read access, one should use the allAccounts() method.
    mutable QMutex accounts_mutex_;
    QVector<Account> accounts_;

#ifdef Q_OS_WIN32
    static bool loadSyncRootInfoCB(struct sqlite3_stmt *stmt, void *data);
    void loadSyncRootInfo();
    void updateSyncRootInfo(SyncRootInfo& sync_root_info);
    const QString getOldSyncRootDir(const Account& account);
    void setAccountSyncRoot(Account &account);

    std::vector<SyncRootInfo> sync_root_infos_;
    QMap<Account, QString> custom_sync_root_names_;
#endif

#ifdef Q_OS_LINUX
    void setAccountDisplayName(Account &account);
#endif
};

#endif  // _SEAF_ACCOUNT_MGR_H
