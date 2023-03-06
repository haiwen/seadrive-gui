#ifndef SEADRIVE_GUI_THUMBNAIL_SERVICE_H
#define SEADRIVE_GUI_THUMBNAIL_SERVICE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QQueue>
#include <QAtomicInt>
#include <QMutex>
#include <QScopedPointer>

#include "utils/singleton.h"
#include "api/requests.h"

class QTimer;

struct ThumbnailRequest;
class GetThumbnailRequest;
class ApiError;

struct ThumbnailRequest {
    static QAtomicInt idgen;
    // Each request has a unique id
    int id;
    Account account;
    QString repo_id;
    QString path;
    int size;

    QString cache_path;

    bool operator==(const ThumbnailRequest &rhs) const
    {
        return repo_id == rhs.repo_id && path == rhs.path && size == rhs.size;
    }
};


// Responsible for fetching of thumbnails. It also handles:
// 1. (TODO) fetching of multiple (up to max_slots) requests concurrently
// 2. (TODO) retrying (up to 3 times, with back off) of failed requests
class ThumbnailDownloader : public QObject
{
    Q_OBJECT
public:
    ThumbnailDownloader(int max_slots=1);
    void download(const ThumbnailRequest &request);
    bool hasFreeSlot() const;

private slots:
    void onGetThumbnailSuccess(const QPixmap &thumbnail);
    void onGetThumbnailFailed(const ApiError &error);

signals:
    // We don't need extra returns when a request finished besides
    // whether it's successful or not, because:
    // 1. For a successful request, the ouptput is determinted, i.e. a local
    // cache file
    // 2. For a failed request, we dont't care about the
    // reason here, as long as it's logged somewhere
    void requestFinished(const ThumbnailRequest &request, bool success);

private:
    int max_slots_;
    QScopedPointer<GetThumbnailRequest, QScopedPointerDeleteLater> api_request_;
    ThumbnailRequest current_request_;
};

struct ThumbnailWaiter;

// Responsible for:
//  * the scheduling of all pending requests
//  * cache mgmt of the thumbnail files
// TODO:
//  * cancel all requests when account is switched
//  * blacklist files whose thumbnail requests fail for too many times (they could be corrupted image files)
class ThumbnailService : public QObject
{
    Q_OBJECT
    SINGLETON_DEFINE(ThumbnailService)
public:
    ThumbnailService();

    void start();

    // Get the thumbnail with the require size. Return immediately if
    // found in local cahce. Otherwise it would block waiting for the
    // api request to finish (or fail). The path to the fetched
    // thumbnail would be saved in the last `file` pointer.
    //
    // **This method is supposed to be called from a worker thread.**
    bool getThumbnail(const Account &account,
                      const QString &repo_id,
                      const QString &path,
                      int size,
                      int timeout_msecs,
                      QString *file);

private slots:
    void onRequestFinished(const ThumbnailRequest &request, bool success);
    void doSchedule();
    void doCleanCache();

private:
    ThumbnailRequest newRequest(const Account &account,
                                const QString &repo_id,
                                const QString &path,
                                int size);
    QString getCacheFilePath(const QString &repo_id,
                             const QString &path,
                             uint size);

    bool getThumbnailFromCache(const QString &repo_id,
                               const QString &path,
                               uint size,
                               QString *file);

    bool enqueueRequest(const ThumbnailRequest& request);

    bool waitForRequest(const ThumbnailRequest &request,
                        int timeout_msecs,
                        QString *file);

    QString thumbnails_dir_;

    ThumbnailDownloader *downloader_;

    QTimer *schedule_timer_;
    QTimer *cache_clean_timer_;

    QQueue<ThumbnailRequest> queue_;
    // The requests queue need to be protected by a mutex because new
    // requests may be added by multiple threads.
    QMutex queue_mutex_;

    QHash<int, ThumbnailWaiter*> waiters_;
    // This mutex protects the waiters_ dict since it could be
    // accessed concurrently by multiple threads.
    QMutex waiters_mutex_;
};

#endif // SEADRIVE_GUI_THUMBNAIL_SERVICE_H
