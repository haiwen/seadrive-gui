#ifndef SEADRIVE_CLIENT_EXT_THUMBNAIL_HANLDER_H
#define SEADRIVE_CLIENT_EXT_THUMBNAIL_HANLDER_H

#include <QObject>
#include <QThread>


#include <windows.h>

#include "utils/singleton.h"

class SeafileRpcClient;
class ExtThumbnailConnectionListenerThread;
class ApiError;

/**
 * Handles command from seafile windows thumbnail shell extension
 */

class WindowsThumbnailExtensionHandler : public QObject {
    SINGLETON_DEFINE(WindowsThumbnailExtensionHandler)
    Q_OBJECT

public:
    WindowsThumbnailExtensionHandler();
    void start();
    void stop();

private slots:
    void onDaemonRestarted();

private:
    ExtThumbnailConnectionListenerThread * listener_thread_;

    bool started_;

};

/**
 * Creates the named pipe and listen for incoming connections in a separate
 * thread.
 *
 * When a connection is accepted mcreate a new ExtCommandsHandler thread to serve it
 */

class ExtThumbnailConnectionListenerThread : public QThread {
  Q_OBJECT
public:
    void run();

private:
    void servePipeInNewThread(HANDLE pipe);

};


/**
 * Serves one windows thumbnail extension connection.
 * "It's an endless loop of "read request" -> "handle request" -> send response"
 */
class ExtThumbnailCommandsHandler : public QThread {
    Q_OBJECT
public:
    ExtThumbnailCommandsHandler(HANDLE pipe);
    void run();

private:
    HANDLE pipe_;
    bool readRequest(QStringList* args);
    bool sendResponse(const QString& resp);
    bool isFileCached(const QString &path);
    bool lookUpFileInformation(const QString &path,
                                QString *ptr_repo_id,
                                QString *ptr_path_in_rpo);
    void handlerFileStatus(QStringList &args, bool* is_cached);
    QString handlerGetDiskLetter();

};

#endif // SEADRIVE_CLIENT_EXT_THUMBNAIL_HANLDER_H
