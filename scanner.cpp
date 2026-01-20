#include "scanner.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QMutexLocker>

Scanner::Scanner(const QString &path, QObject *parent)
    : QObject(parent)
    , m_rootPath(path)
    , m_running(false)
    , m_cancelRequested(false)
    , m_totalFiles(0)
    , m_scannedFiles(0)
    , m_totalSize(0)
    , m_activeTasks(0)
{
    // Оптимальное количество потоков
    int threadCount = QThread::idealThreadCount();
    m_threadPool.setMaxThreadCount(threadCount > 2 ? threadCount : 2);
    qDebug() << "Scanner создан, потоков:" << m_threadPool.maxThreadCount();
}

Scanner::~Scanner()
{
    stop();
    m_threadPool.waitForDone();
}

void Scanner::start()
{
    if (m_running) {
        qDebug() << "Сканер уже запущен";
        return;
    }

    m_running = true;
    m_cancelRequested = false;
    m_totalFiles = 0;
    m_scannedFiles = 0;
    m_totalSize = 0;
    m_activeTasks = 0;

    qDebug() << "Запуск сканирования:" << m_rootPath;

    // Создаем корневой элемент
    QFileInfo rootInfo(m_rootPath);
    m_rootItem = std::make_shared<FileItem>(
        rootInfo.fileName().isEmpty() ? rootInfo.absoluteFilePath() : rootInfo.fileName(),
        m_rootPath,
        0,
        rootInfo.lastModified(),
        true
    );

    // Сначала считаем общее количество файлов для прогресса
    QtConcurrent::run(&m_threadPool, [this]() {
        try {
            m_totalFiles = countFilesInDirectory(m_rootPath);
            qDebug() << "Всего файлов для сканирования:" << m_totalFiles;

            emit progress(0, m_rootPath, 0, 0);

            // Запускаем сканирование
            if (!m_cancelRequested) {
                scanDirectory(m_rootPath, m_rootItem);
            }
        } catch (const std::exception& e) {
            qDebug() << "Ошибка при сканировании:" << e.what();
            emit error(QString("Ошибка сканирования: %1").arg(e.what()));
        }
    });
}

void Scanner::stop()
{
    if (!m_running) return;

    qDebug() << "Запрос остановки сканирования";
    m_cancelRequested = true;

    // Ждем завершения всех задач
    int timeout = 0;
    while (m_activeTasks > 0 && timeout < 50) { // 5 секунд максимум
        QThread::msleep(100);
        QApplication::processEvents();
        timeout++;
    }

    if (m_activeTasks > 0) {
        qDebug() << "Принудительная очистка очереди задач";
        m_threadPool.clear(); // Очищаем очередь задач
    }

    m_running = false;
    qDebug() << "Scanner остановлен, активных задач:" << m_activeTasks;
}

int Scanner::countFilesInDirectory(const QString &path)
{
    if (m_cancelRequested) {
        qDebug() << "Подсчет файлов прерван";
        return 0;
    }

    int count = 0;
    QDir dir(path);

    if (!dir.exists()) {
        qDebug() << "Директория не существует:" << path;
        return 0;
    }

    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::Name
    );

    for (const QFileInfo &entry : entries) {
        if (m_cancelRequested) {
            qDebug() << "Подсчет файлов прерван по запросу";
            break;
        }

        if (entry.isDir() && !entry.isSymLink()) {
            count += countFilesInDirectory(entry.absoluteFilePath());
        } else if (entry.isFile()) {
            count++;
        }

        // Периодически обрабатываем события
        if (count % 1000 == 0) {
            QApplication::processEvents();
        }
    }

    return count;
}

void Scanner::scanDirectory(const QString &path, std::shared_ptr<FileItem> parent)
{
    if (m_cancelRequested) {
        qDebug() << "Сканирование прервано:" << path;
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        qDebug() << "Директория не существует:" << path;
        emit error("Директория не существует: " + path);
        return;
    }

    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::DirsFirst | QDir::Name
    );

    // Обрабатываем файлы в текущем потоке
    for (const QFileInfo &entry : entries) {
        if (m_cancelRequested) {
            qDebug() << "Обработка прервана:" << path;
            break;
        }

        if (entry.isFile()) {
            auto fileItem = std::make_shared<FileItem>(
                entry.fileName(),
                entry.absoluteFilePath(),
                entry.size(),
                entry.lastModified(),
                false
            );

            {
                QMutexLocker locker(&m_mutex);
                parent->addChild(fileItem);
                m_scannedFiles++;
                m_totalSize += entry.size();
            }

            emit fileFound(entry.absoluteFilePath(), entry.size());

            // Обновляем прогресс
            int percent = m_totalFiles > 0 ? (m_scannedFiles * 100) / m_totalFiles : 0;
            emit progress(percent, entry.absoluteFilePath(), m_scannedFiles, m_totalSize);

        } else if (entry.isDir() && !entry.isSymLink()) {
            // Создаем элемент для директории
            auto dirItem = std::make_shared<FileItem>(
                entry.fileName(),
                entry.absoluteFilePath(),
                0,
                entry.lastModified(),
                true
            );

            {
                QMutexLocker locker(&m_mutex);
                parent->addChild(dirItem);
            }

            // Запускаем сканирование поддиректории в отдельной задаче
            if (!m_cancelRequested) {
                m_activeTasks++;
                QtConcurrent::run(&m_threadPool, [this, entry, dirItem]() {
                    try {
                        scanDirectory(entry.absoluteFilePath(), dirItem);
                    } catch (const std::exception& e) {
                        qDebug() << "Ошибка при сканировании" << entry.absoluteFilePath() << ":" << e.what();
                    }
                    m_activeTasks--;

                    // Если это была последняя задача, отправляем сигнал завершения
                    if (m_activeTasks == 0 && !m_cancelRequested) {
                        QMetaObject::invokeMethod(this, &Scanner::onTaskFinished, Qt::QueuedConnection);
                    }
                });
            }
        }

        // Позволяем обрабатывать события
        if (entries.size() > 50) {
            QApplication::processEvents();
        }
    }

    // Если это корневая директория и нет активных задач, завершаем
    if (path == m_rootPath && m_activeTasks == 0 && !m_cancelRequested) {
        onTaskFinished();
    }
}

void Scanner::onTaskFinished()
{
    qDebug() << "Все задачи завершены, отправка сигнала finished";

    if (m_cancelRequested) {
        m_running = false;
        emit progress(100, "Отменено", m_scannedFiles, m_totalSize);
        qDebug() << "Сканирование отменено. Файлов:" << m_scannedFiles << "Размер:" << m_totalSize;
    } else {
        emit progress(100, "Завершено", m_scannedFiles, m_totalSize);
        emit finished(m_rootItem);
        qDebug() << "Сканирование завершено. Файлов:" << m_scannedFiles << "Размер:" << m_totalSize;
    }

    m_running = false;
}
