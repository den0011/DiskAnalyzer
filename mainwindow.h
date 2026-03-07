#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "fileitem.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Scanner;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBrowseClicked();
    void onScanClicked();
    void onStopClicked();

    void onScannerProgress(int percent, const QString &path, int filesCount, qint64 totalSize);
    void onScannerFileFound(const QString &filePath, qint64 size);
    void onScannerFinished(std::shared_ptr<FileItem> root);
    void onScannerError(const QString &message);

    void updateVisualizations();

    // Слоты для контекстного меню таблицы
    void onFilesTableCustomContextMenuRequested(const QPoint &pos);
    void openSelectedFile();
    void openSelectedFileLocation();
    void showFileProperties();
    void copyFilePath();
    void copyFileName();

private:
    void setupUi();
    void setupConnections();
    void updateChart(std::shared_ptr<FileItem> root);
    void updateLargestFiles(std::shared_ptr<FileItem> root);
    void collectFiles(std::shared_ptr<FileItem> item, QList<std::shared_ptr<FileItem>> &files);
    QString formatSize(qint64 bytes) const;

    // Вспомогательные методы для работы с выделенными файлами
    QList<std::shared_ptr<FileItem>> getSelectedFiles() const;
    std::shared_ptr<FileItem> getFirstSelectedFile() const;

    Ui::MainWindow *ui;
    Scanner *m_scanner;
    std::shared_ptr<FileItem> m_rootItem;

    QList<std::shared_ptr<FileItem>> m_allFiles;  // Все файлы для быстрого доступа
    QTimer *m_updateTimer;
    bool m_isScanning;
};

#endif // MAINWINDOW_H
