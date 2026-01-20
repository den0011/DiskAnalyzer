#ifndef SCANNER_H
#define SCANNER_H

#include <QObject>
#include <QThreadPool>
#include <QtConcurrent>
#include <atomic>
#include <memory>
#include "fileitem.h"

class Scanner : public QObject
{
    Q_OBJECT

public:
    explicit Scanner(const QString &path, QObject *parent = nullptr);
    ~Scanner();

    void start();
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void progress(int percent, const QString &currentPath, int filesCount, qint64 totalSize);
    void fileFound(const QString &filePath, qint64 size);
    void finished(std::shared_ptr<FileItem> root);
    void error(const QString &message);

private slots:
    void onTaskFinished();

private:
    void scanDirectory(const QString &path, std::shared_ptr<FileItem> parent);
    int countFilesInDirectory(const QString &path);

    QString m_rootPath;
    std::atomic<bool> m_running;
    std::atomic<bool> m_cancelRequested;
    std::atomic<int> m_totalFiles;
    std::atomic<int> m_scannedFiles;
    std::atomic<qint64> m_totalSize;
    std::atomic<int> m_activeTasks;

    QThreadPool m_threadPool;
    std::shared_ptr<FileItem> m_rootItem;
    QMutex m_mutex;
};

#endif // SCANNER_H
