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

typedef enum {
    AccountAdded = 0,
    AccountRemoved,
    AccountResynced,
} MessageType;

struct AccountMessage {
    MessageType type;
    Account account;
};

#if defined(_MSC_VER)
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

#if defined(_MSC_VER)
    void loadSyncRootInfo();
#endif
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

#if defined(Q_OS_WIN32)
    // Resync the account. Used when user resync an account from the
    // account menu.
    int resyncAccount(const Account& account);
#endif

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

    // messages serves as a simple asynchronous queue between account
    // adding/removing events and rpc calls to daemon. One should enqueue
    // the message first, and then emit the accountMQUpdated() signal.
    QQueue<AccountMessage> messages;

public slots:
    void reloginAccount(const Account &account);

signals:
    /**
     * Account added/removed/switched.
     */
    void accountMQUpdated();
    void accountInfoUpdated(const Account& account);

private slots:
    void slotUpdateAccountInfoSucess(const AccountInfo& info);
    void slotUpdateAccountInfoFailed();
    void serverInfoSuccess(const Account &account, const ServerInfo &info);
    void serverInfoFailed(const ApiError&);

private:
    Q_DISABLE_COPY(AccountManager)

    void fetchAccountInfoFromServer(const Account& account);
    void updateAccountServerInfo(const Account& account);
#if defined(_MSC_VER)
    static bool loadSyncRootInfoCB(struct sqlite3_stmt *stmt, void *data);
    const QString getOldSyncRootDir(const Account& account);
    const QString genSyncRootName(const Account& account, bool resync);
    void setAccountSyncRoot(Account &account, bool resync);
#endif
    static bool loadAccountsCB(struct sqlite3_stmt *stmt, void *data);
    static bool loadServerInfoCB(struct sqlite3_stmt *stmt, void *data);

    void updateAccountLastVisited(const Account& account);
#if defined(_MSC_VER)
    void updateSyncRootInfo(SyncRootInfo& sync_root_info);
#endif
    Account getAccount(const QString& url, const QString& username) const;
    void addAccountToDaemon(const Account& account);

    struct sqlite3 *db;

    // accounts_ will be accessed by multiple threads, thus it should be protected by the accounts_mutex_.
    // For read access, one should use the allAccounts() method.
    mutable QMutex accounts_mutex_;
    QVector<Account> accounts_;

#if defined(_MSC_VER)
    // Store All sync root information
    std::vector<SyncRootInfo> sync_root_infos_;
#endif
};

#endif  // _SEAF_ACCOUNT_MGR_H
