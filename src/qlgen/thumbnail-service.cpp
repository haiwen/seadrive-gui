#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QImage>
#include <QQueue>
#include <QTimer>
#include <QSemaphore>
#include <QThread>
#include <QMutexLocker>
#include <QThreadPool>
#include <QDirIterator>

#include "account-mgr.h"
#include "api/api-error.h"
#include "api/requests.h"
#include "seadrive-gui.h"
#include "utils/file-utils.h"
#include "utils/utils.h"

#include "thumbnail-service.h"

namespace {

// If a cached thumbnail is older than this time, we would re-request
// it from the server.
const int kThumbCacheValidSecs = 60;
// How often do we run the cache cleaner to purge expired thumb
// caches.
const int kThumbCacheCleanIntervalSecs = 300;

// Internal scheduling time to check if there is queued requests.
const int kScheduleIntervalSecs = 1;

class FileTimeComparator {
public:
    FileTimeComparator(const QFileInfo& info): finfo_(info) {
    }

    bool isOlderThan(qint64 threshold_secs) const {
        return finfo_.lastModified().secsTo(QDateTime::currentDateTime()) > threshold_secs;
    }
private:
    const QFileInfo finfo_;
};

} // namespace

struct ThumbnailWaiter {
    QSemaphore sem;
    bool success;
};

QAtomicInt ThumbnailRequest::idgen;

SINGLETON_IMPL(ThumbnailService)

ThumbnailService::ThumbnailService()
{
    schedule_timer_ = new QTimer(this);
    connect(schedule_timer_, SIGNAL(timeout()),
            this, SLOT(doSchedule()));

    cache_clean_timer_ = new QTimer(this);
    connect(cache_clean_timer_, SIGNAL(timeout()),
            this, SLOT(doCleanCache()));

    downloader_ = new ThumbnailDownloader();
    connect(downloader_,
            SIGNAL(requestFinished(const ThumbnailRequest &, bool)),
            this,
            SLOT(onRequestFinished(const ThumbnailRequest &, bool)));
}

ThumbnailRequest ThumbnailService::newRequest(const QString &repo_id,
                                              const QString &path,
                                              int size)
{
    ThumbnailRequest req;
    // `ThumbnailRequest::idgen` is a `QAtomicInt`, whose ++ operator
    // (or `fetchAndAddOrdered` function for Qt < 5.3) is overloaded
    // to be an atomic action. So no lock required for it, even if
    // this function could be called from diffrent threads
    // concurrently.
    req.id = ThumbnailRequest::idgen.fetchAndAddOrdered(1);
    req.repo_id = repo_id;
    req.path = path;
    req.size = size;
    req.cache_path = getCacheFilePath(repo_id, path, size);
    return req;
}


QString ThumbnailService::getCacheFilePath(const QString &repo_id,
                                           const QString &path,
                                           uint size)
{
    QString size_str = QString::number(size);
    return QDir(thumbnails_dir_)
        .filePath(::md5(repo_id + path) + "-" + size_str + ".png");
}

bool ThumbnailService::getThumbnailFromCache(const QString &repo_id,
                                             const QString &path,
                                             uint size,
                                             QString *file)
{
    QString cached_file = getCacheFilePath(repo_id, path, size);
    // printf ("cache file path = %s\n", toCStr(cached_file));
    QFileInfo finfo(cached_file);
    if (!finfo.exists()) {
        return false;
    }

    // We only keep the cached thumbnail valid for a small period of
    // time (no more that a few minutes), because:
    // 1) For a file, its thumbnail cache key has no information of
    //    the file id (a.k.a version). So we keep the cache very short
    //    to avoid stale thumbnails in case the file changes.
    // 2) quicklookd itself also maintains a cache for all the
    //    thumbnails, and it only re-request the thumbnail for a file
    //    when it detects a timestamp change for that file.
    //
    //  You may ask: Now that quicklookd does the cache, is it still
    //  necessary to have our own cache? The answer is YES. Because:
    //  1) Sometimes the thumbnail api request could take too long so
    //     the ql generator may time out waiting for our response. But
    //     soon it may request the thumbnail again. In such case the
    //     cache would greatly speed it up.
    //  2) We have to save the thumbnail returned by the api to a
    //     local file anyway - See genThumnail method in qlgen.mm,
    //     especially the call to `QLThumbnailRequestSetImageAtURL`.
    if (FileTimeComparator(finfo).isOlderThan(kThumbCacheValidSecs)) {
        return false;
    }
    *file = cached_file;
    return true;
}

void ThumbnailService::start()
{
    thumbnails_dir_ = QDir(gui->seadriveDir()).filePath("thumbs");
    checkdir_with_mkdir(toCStr(thumbnails_dir_));
    schedule_timer_->start(kScheduleIntervalSecs * 1000);
    cache_clean_timer_->start(kThumbCacheCleanIntervalSecs * 1000);
}

bool ThumbnailService::getThumbnail(const QString &repo_id,
                                    const QString &path,
                                    int size,
                                    int timeout_msecs,
                                    QString *file)
{
    // printf ("ThumbnailService::getThumbnail called\n");

    if (getThumbnailFromCache(repo_id, path, size, file)) {
        // Cache hit
        return true;
    }

    // TODO: handle the case when there is already a request for this
    // file+size
    ThumbnailRequest request = newRequest(repo_id, path, size);

    enqueueRequest(request);

    return waitForRequest(request, timeout_msecs, file);
}

bool ThumbnailService::enqueueRequest(const ThumbnailRequest& request)
{
    QMutexLocker lock(&queue_mutex_);
    queue_.enqueue(request);
    return true;
}

bool ThumbnailService::waitForRequest(const ThumbnailRequest& request, int timeout_msecs, QString *file)
{
    ThumbnailWaiter *waiter = new ThumbnailWaiter();
    {
        QMutexLocker lock(&waiters_mutex_);
        waiters_[request.id] = waiter;
    }

    bool ret = waiter->sem.tryAcquire(1, timeout_msecs);
    // printf ("waiting for result\n");
    if (ret && waiter->success) {
        *file = getCacheFilePath(request.repo_id, request.path, request.size);
        // *file = "/data/github/seadrive-gui/qlgen/test.jpg";
    }
    // printf ("got result %s\n", ret ? "success" : "timeout");
    {
        QMutexLocker lock(&waiters_mutex_);
        waiters_.remove(request.id);
    }
    return ret;
}

// All requests are kick-started here in the main thread (as a
// callback of a timer). This way the downloader is only accessed from
// the main thread to avoid using any locks for it.
void ThumbnailService::doSchedule()
{
    // printf ("downloader has free slot: %s\n", downloader_->hasFreeSlot() ? "yes" : "no");
    // printf ("total thumb requests in queue: %d\n", queue_.size());
    // foreach (const ThumbnailRequest& t, queue_) {
    //     printf ("queued thumb request: %s\n", toCStr(t.path));
    // }

    if (!downloader_->hasFreeSlot()) {
        return;
    }
    QMutexLocker lock(&queue_mutex_);
    if (queue_.isEmpty()) {
        return;
    }
    ThumbnailRequest request = queue_.dequeue();
    downloader_->download(request);
}

class ThumbCacheCleaner : public QRunnable {
public:
    ThumbCacheCleaner(const QString& cache_dir) : cache_dir_(cache_dir) {};

    void run() {
        // printf ("checking out dated cache files in %s\n", toCStr(cache_dir_));
        QDirIterator iterator(cache_dir_);
        QStringList files_to_delete;
        while (iterator.hasNext()) {
            iterator.next();
            QString file_path = iterator.filePath();
            QFileInfo finfo(file_path);
            if (!finfo.isFile()) {
                continue;
            }
            if (FileTimeComparator(finfo).isOlderThan(kThumbCacheValidSecs)) {
                files_to_delete.push_back(file_path);
            }
        }
        foreach (const QString& file_path, files_to_delete) {
            // printf ("removing file %s\n", toCStr(file_path));

            // TODO: there may be a race condition since the cleaner
            // runs in a worker thread while we serve requets from
            // SeariveQL in another thread.
            QFile(file_path).remove();
        }
    }
private:
    const QString cache_dir_;
};

void ThumbnailService::doCleanCache()
{
    ThumbCacheCleaner *cleaner = new ThumbCacheCleaner(thumbnails_dir_);
    QThreadPool::globalInstance()->start(cleaner);
}

void ThumbnailService::onRequestFinished(const ThumbnailRequest &request, bool success)
{
    QMutexLocker lock(&waiters_mutex_);
    if (!waiters_.contains(request.id)) {
        return;
    }
    ThumbnailWaiter *waiter = waiters_[request.id];
    waiter->success = success;
    lock.unlock();
    waiter->sem.release();
    doSchedule();
}

ThumbnailDownloader::ThumbnailDownloader(int max_slots)
    : max_slots_(max_slots)
{
}

bool ThumbnailDownloader::hasFreeSlot() const
{
    return !api_request_;
}

void ThumbnailDownloader::download(const ThumbnailRequest& request)
{
    // printf("starting to download request %d\n", request.id);
    current_request_ = request;
    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        emit requestFinished(current_request_, false);
        return;
    }
    api_request_.reset(new GetThumbnailRequest(account,
                                               request.repo_id,
                                               request.path,
                                               request.size));
    connect(api_request_.data(), SIGNAL(success(const QPixmap&)),
            this, SLOT(onGetThumbnailSuccess(const QPixmap&)));
    connect(api_request_.data(), SIGNAL(failed(const ApiError&)),
            this, SLOT(onGetThumbnailFailed(const ApiError&)));

    api_request_->send();
}


void ThumbnailDownloader::onGetThumbnailSuccess(const QPixmap &thumbnail)
{
    api_request_.reset(nullptr);
    // TODO: Saving image to local file should be done in a worker thread!
    if (thumbnail.save(current_request_.cache_path)) {
        emit requestFinished(current_request_, true);
    } else {
        // Failed to save the thumb to local file
        emit requestFinished(current_request_, false);
    }
}

void ThumbnailDownloader::onGetThumbnailFailed(const ApiError &error)
{
    api_request_.reset(nullptr);
    emit requestFinished(current_request_, false);
}
