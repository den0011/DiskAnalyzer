#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "scanner.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <algorithm>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QClipboard>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_scanner(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_isScanning(false)
{
    ui->setupUi(this);
    setupConnections();

    setWindowTitle("Анализатор дискового пространства");
    resize(1200, 800);

    // Настройка таблицы
    ui->filesTable->setColumnCount(4);
    ui->filesTable->setHorizontalHeaderLabels({"Имя файла", "Размер", "Путь", "Дата изменения"});
    ui->filesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->filesTable->verticalHeader()->setDefaultSectionSize(20);
    ui->filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->filesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->filesTable->setSortingEnabled(true);
    ui->filesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->filesTable->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Настройка сортировки по размеру по умолчанию (по убыванию)
    ui->filesTable->sortByColumn(1, Qt::DescendingOrder);

    // Таймер для обновления визуализаций
    m_updateTimer->setInterval(1000);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateVisualizations);

    ui->pathEdit->setText(QDir::homePath());

    // Инициализация QChartView
    ui->chartView->setRenderHint(QPainter::Antialiasing);
}

MainWindow::~MainWindow()
{
    if (m_scanner) {
        m_scanner->stop();
        m_scanner->deleteLater();
    }
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(ui->scanBtn, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);

    // Контекстное меню таблицы
    connect(ui->filesTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::onFilesTableCustomContextMenuRequested);

    // Двойной клик по строке таблицы
    connect(ui->filesTable, &QTableWidget::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid()) {
            openSelectedFile();
        }
    });
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Выберите директорию для анализа",
        ui->pathEdit->text()
    );

    if (!dir.isEmpty()) {
        ui->pathEdit->setText(dir);
    }
}

void MainWindow::onScanClicked()
{
    if (m_isScanning) {
        qDebug() << "Сканирование уже выполняется";
        return;
    }

    QString path = ui->pathEdit->text().trimmed();
    if (path.isEmpty() || !QDir(path).exists()) {
        QMessageBox::warning(this, "Ошибка", "Укажите существующий путь для сканирования");
        return;
    }

    qDebug() << "Начинаем сканирование:" << path;

    // Очистка предыдущих результатов
    ui->filesTable->setRowCount(0);
    m_allFiles.clear();

    // Создаем сканер
    if (m_scanner) {
        m_scanner->deleteLater();
    }

    m_scanner = new Scanner(path, this);
    connect(m_scanner, &Scanner::progress, this, &MainWindow::onScannerProgress);
    connect(m_scanner, &Scanner::fileFound, this, &MainWindow::onScannerFileFound);
    connect(m_scanner, &Scanner::finished, this, &MainWindow::onScannerFinished);
    connect(m_scanner, &Scanner::error, this, &MainWindow::onScannerError);

    // Настройка UI
    ui->scanBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->progressBar->setVisible(true);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->statusLabel->setText("Подсчет файлов...");

    m_isScanning = true;
    m_updateTimer->start();

    // Запуск сканирования
    qDebug() << "Запускаем сканер...";
    m_scanner->start();
}

void MainWindow::onStopClicked()
{
    if (m_scanner && m_isScanning) {
        qDebug() << "Останавливаем сканирование...";
        ui->statusLabel->setText("Остановка сканирования...");
        ui->stopBtn->setEnabled(false);
        m_scanner->stop();
        m_isScanning = false;
        ui->scanBtn->setEnabled(true);
        ui->progressBar->setVisible(false);
    }
}

void MainWindow::onScannerProgress(int percent, const QString &path, int filesCount, qint64 totalSize)
{
    ui->progressBar->setValue(percent);

    QString status = QString("Сканирование: %1% | Файлов: %2 | Размер: %3")
                        .arg(percent)
                        .arg(filesCount)
                        .arg(formatSize(totalSize));

    if (!path.isEmpty() && path.length() < 50) {
        QString fileName = QFileInfo(path).fileName();
        if (!fileName.isEmpty()) {
            status += " | " + fileName;
        }
    }

    ui->statusLabel->setText(status);

    // Обновляем заголовок окна
    setWindowTitle(QString("Анализатор дискового пространства - %1%").arg(percent));

    // Обновляем таблицу каждые 100 файлов
    static int lastUpdateCount = 0;
    if (filesCount - lastUpdateCount >= 100) {
        if (m_rootItem) {
            updateLargestFiles(m_rootItem);
        }
        lastUpdateCount = filesCount;
    }
}

void MainWindow::onScannerFileFound(const QString &filePath, qint64 size)
{

    static int fileCounter = 0;
    fileCounter++;

    // Обновляем UI каждые 500 файлов
    if (fileCounter % 500 == 0) {
        QApplication::processEvents();
    }
}

void MainWindow::onScannerFinished(std::shared_ptr<FileItem> root)
{
    qDebug() << "Сканирование завершено";

    m_rootItem = root;
    m_isScanning = false;
    m_updateTimer->stop();

    ui->progressBar->setVisible(false);
    ui->scanBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);

    if (root) {
        // Собираем все файлы в m_allFiles для быстрого доступа
        m_allFiles.clear();
        collectFiles(root, m_allFiles);

        qint64 totalSize = root->totalSize();
        ui->statusLabel->setText(
            QString("Готово. Всего: %1 | Файлов: %2")
                .arg(formatSize(totalSize))
                .arg(m_allFiles.size())
        );

        qDebug() << "Обновляем визуализации...";

        // Обновляем визуализации
        updateChart(root);
        updateLargestFiles(root);
    } else {
        ui->statusLabel->setText("Сканирование отменено");
    }

    setWindowTitle("Анализатор дискового пространства");

    // Очищаем сканер
    if (m_scanner) {
        m_scanner->deleteLater();
        m_scanner = nullptr;
    }

    qDebug() << "Обработка завершения сканирования окончена";
}

void MainWindow::onScannerError(const QString &message)
{
    qDebug() << "Ошибка сканирования:" << message;
    QMessageBox::critical(this, "Ошибка сканирования", message);
    onStopClicked();
}

void MainWindow::updateVisualizations()
{
    // Периодическое обновление визуализаций во время сканирования
    if (m_rootItem && !m_isScanning) {
        // Если сканирование завершено, обновляем все
        updateChart(m_rootItem);
        updateLargestFiles(m_rootItem);
    }
}

void MainWindow::updateChart(std::shared_ptr<FileItem> root)
{
    if (!root || root->children().isEmpty()) {
        // Создаем пустую диаграмму
        auto chart = new QtCharts::QChart();
        chart->setTitle("Распределение дискового пространства");
        chart->legend()->hide();

        auto series = new QtCharts::QPieSeries();
        series->append("Нет данных", 1);
        chart->addSeries(series);

        ui->chartView->setChart(chart);
        return;
    }

    // Создаем круговую диаграмму из дочерних элементов корня
    auto chart = new QtCharts::QChart();
    chart->setTitle("Распределение дискового пространства");
    chart->legend()->setAlignment(Qt::AlignRight);
    chart->setAnimationOptions(QtCharts::QChart::SeriesAnimations);

    auto series = new QtCharts::QPieSeries();

    // Берем топ-8 самых крупных элементов
    auto children = root->children();
    qint64 totalSize = root->totalSize();

    if (totalSize == 0) {
        series->append("Нет данных", 1);
        chart->addSeries(series);
        ui->chartView->setChart(chart);
        return;
    }

    // Сортируем по размеру
    std::sort(children.begin(), children.end(),
              [](const auto &a, const auto &b) {
                  return a->totalSize() > b->totalSize();
              });

    int count = 0;
    qint64 othersSize = 0;

    for (const auto &child : children) {
        if (count < 8 && child->totalSize() > 0) {
            qreal percentage = (child->totalSize() * 100.0) / totalSize;
            if (percentage >= 0.5) { // Показываем только если > 0.5%
                auto slice = series->append(
                    QString("%1\n%2%")
                        .arg(child->name())
                        .arg(percentage, 0, 'f', 1),
                    child->totalSize()
                );
                slice->setLabelVisible(percentage > 2.0);
                count++;
            } else {
                othersSize += child->totalSize();
            }
        } else {
            othersSize += child->totalSize();
        }
    }

    // Добавляем категорию "Другие"
    if (othersSize > 0) {
        qreal percentage = (othersSize * 100.0) / totalSize;
        if (percentage > 0) {
            auto slice = series->append(
                QString("Другие\n%1%").arg(percentage, 0, 'f', 1),
                othersSize
            );
            slice->setLabelVisible(percentage > 2.0);
        }
    }

    if (series->count() == 0) {
        series->append("Нет данных", 1);
    }

    chart->addSeries(series);
    ui->chartView->setChart(chart);
}

/*
void MainWindow::updateLargestFiles(std::shared_ptr<FileItem> root)
{
    if (!root) return;

    // Если файлы уже собраны в m_allFiles (в onScannerFinished), используем их
    if (m_allFiles.isEmpty()) {
        collectFiles(root, m_allFiles);
    }

    // Сортируем по размеру (по убыванию)
    std::sort(m_allFiles.begin(), m_allFiles.end(),
              [](const auto &a, const auto &b) {
                  return a->size() > b->size();
              });

    // Отображаем топ-100
    int count = qMin(100, m_allFiles.size());
    ui->filesTable->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const auto &file = m_allFiles[i];

        // Проверяем, что у нас есть все необходимые данные
        if (!file) continue;

        // Заполняем все 4 колонки данными, даже если некоторые пустые
        ui->filesTable->setItem(i, 0, new QTableWidgetItem(
            !file->name().isEmpty() ? file->name() : "Неизвестно"
        ));

        ui->filesTable->setItem(i, 1, new QTableWidgetItem(
            formatSize(file->size())
        ));

        ui->filesTable->setItem(i, 2, new QTableWidgetItem(
            !file->path().isEmpty() ? file->path() : "-"
        ));

        ui->filesTable->setItem(i, 3, new QTableWidgetItem(
            file->modified().isValid() ?
            file->modified().toString("dd.MM.yyyy HH:mm") :
            "-"
        ));
    }

    ui->filesTable->resizeColumnsToContents();
    ui->filesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    // Восстанавливаем сортировку по размеру (по убыванию)
    ui->filesTable->sortByColumn(1, Qt::DescendingOrder);
}
*/

void MainWindow::updateLargestFiles(std::shared_ptr<FileItem> root)
{
    if (!root) return;

    // Если файлы уже собраны в m_allFiles (в onScannerFinished), используем их
    if (m_allFiles.isEmpty()) {
        collectFiles(root, m_allFiles);
    }

    // Сортируем по размеру (по убыванию)
    std::sort(m_allFiles.begin(), m_allFiles.end(),
              [](const auto &a, const auto &b) {
                  return a->size() > b->size();
              });

    // Временно отключаем сортировку для корректного заполнения
    ui->filesTable->setSortingEnabled(false);

    // Отображаем топ-100
    int count = qMin(100, m_allFiles.size());
    ui->filesTable->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const auto &file = m_allFiles[i];

        if (!file) {
            qDebug() << "Пустой файл на позиции" << i;
            continue;
        }

        // Колонка 0: Имя файла
        QTableWidgetItem *nameItem = new QTableWidgetItem(
            !file->name().isEmpty() ? file->name() : "Неизвестно"
        );
        ui->filesTable->setItem(i, 0, nameItem);

        // Колонка 1: Размер (сохраняем сырой размер для сортировки)
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatSize(file->size()));
        sizeItem->setData(Qt::UserRole, file->size()); // Сохраняем числовое значение
        ui->filesTable->setItem(i, 1, sizeItem);

        // Колонка 2: Путь
        QTableWidgetItem *pathItem = new QTableWidgetItem(
            !file->path().isEmpty() ? file->path() : "-"
        );
        ui->filesTable->setItem(i, 2, pathItem);

        // Колонка 3: Дата изменения
        QString dateStr = "-";
        if (file->modified().isValid()) {
            dateStr = file->modified().toString("dd.MM.yyyy HH:mm");
        }
        QTableWidgetItem *dateItem = new QTableWidgetItem(dateStr);
        dateItem->setData(Qt::UserRole, file->modified()); // Сохраняем дату для сортировки
        ui->filesTable->setItem(i, 3, dateItem);
    }

    ui->filesTable->resizeColumnsToContents();
    ui->filesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    // Включаем сортировку обратно и сортируем по размеру (по убыванию)
    ui->filesTable->setSortingEnabled(true);
    ui->filesTable->sortByColumn(1, Qt::DescendingOrder);
}


void MainWindow::collectFiles(std::shared_ptr<FileItem> item, QList<std::shared_ptr<FileItem>> &files)
{
    if (!item) return;

    if (!item->isDirectory()) {
        files.append(item);
        return;
    }

    for (const auto &child : item->children()) {
        collectFiles(child, files);
    }
}

QString MainWindow::formatSize(qint64 bytes) const
{
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;
    constexpr qint64 GB = MB * 1024;
    constexpr qint64 TB = GB * 1024;

    if (bytes == 0) return "0 Б";

    if (bytes >= TB)
        return QString("%1 TB").arg(bytes / static_cast<double>(TB), 0, 'f', 2);
    else if (bytes >= GB)
        return QString("%1 GB").arg(bytes / static_cast<double>(GB), 0, 'f', 2);
    else if (bytes >= MB)
        return QString("%1 MB").arg(bytes / static_cast<double>(MB), 0, 'f', 2);
    else if (bytes >= KB)
        return QString("%1 KB").arg(bytes / static_cast<double>(KB), 0, 'f', 2);
    else
        return QString("%1 Б").arg(bytes);
}



void MainWindow::onFilesTableCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->filesTable->indexAt(pos);
    if (!index.isValid()) return;

    QMenu contextMenu(this);

    // Определяем количество выделенных файлов
    QList<QTableWidgetItem*> selectedItems = ui->filesTable->selectedItems();
    int selectedRows = 0;
    if (!selectedItems.isEmpty()) {
        QSet<int> rows;
        for (auto item : selectedItems) {
            rows.insert(item->row());
        }
        selectedRows = rows.size();
    }

    // Добавляем действия в меню
    QAction *openAction = contextMenu.addAction(
        selectedRows == 1 ? "Открыть файл" : "Открыть файлы",
        this, &MainWindow::openSelectedFile
    );

    QAction *openLocationAction = contextMenu.addAction(
        selectedRows == 1 ? "Открыть расположение файла" : "Открыть расположения файлов",
        this, &MainWindow::openSelectedFileLocation
    );

    contextMenu.addSeparator();

    QAction *propertiesAction = contextMenu.addAction(
        selectedRows == 1 ? "Свойства файла" : "Свойства файлов",
        this, &MainWindow::showFileProperties
    );

    contextMenu.addSeparator();

    QAction *copyPathAction = contextMenu.addAction(
        "Копировать путь",
        this, &MainWindow::copyFilePath
    );

    QAction *copyNameAction = contextMenu.addAction(
        "Копировать имя файла",
        this, &MainWindow::copyFileName
    );

    // Показываем контекстное меню
    contextMenu.exec(ui->filesTable->viewport()->mapToGlobal(pos));
}

QList<std::shared_ptr<FileItem>> MainWindow::getSelectedFiles() const
{
    QList<std::shared_ptr<FileItem>> selectedFiles;

    QModelIndexList selectedIndexes = ui->filesTable->selectionModel()->selectedRows(0);
    for (const QModelIndex &index : selectedIndexes) {
        int row = index.row();
        if (row >= 0 && row < m_allFiles.size()) {
            selectedFiles.append(m_allFiles[row]);
        }
    }

    return selectedFiles;
}

std::shared_ptr<FileItem> MainWindow::getFirstSelectedFile() const
{
    QModelIndexList selectedIndexes = ui->filesTable->selectionModel()->selectedRows(0);
    if (!selectedIndexes.isEmpty()) {
        int row = selectedIndexes.first().row();
        if (row >= 0 && row < m_allFiles.size()) {
            return m_allFiles[row];
        }
    }
    return nullptr;
}

void MainWindow::openSelectedFile()
{
    auto selectedFiles = getSelectedFiles();

    for (const auto &file : selectedFiles) {
        if (file && QFile::exists(file->path())) {
            QUrl fileUrl = QUrl::fromLocalFile(file->path());
            if (!QDesktopServices::openUrl(fileUrl)) {
                QMessageBox::warning(this, "Ошибка",
                    QString("Не удалось открыть файл:\n%1").arg(file->path()));
            }
        } else {
            QMessageBox::warning(this, "Ошибка",
                QString("Файл не существует:\n%1").arg(file ? file->path() : ""));
        }
    }
}

void MainWindow::openSelectedFileLocation()
{
    auto selectedFiles = getSelectedFiles();

    // Для нескольких файлов открываем общую директорию первого файла
    if (!selectedFiles.isEmpty()) {
        QString dirPath;

        if (selectedFiles.size() == 1) {
            // Для одного файла - его директорию
            dirPath = QFileInfo(selectedFiles.first()->path()).absolutePath();
        } else {
            // Для нескольких файлов - директорию первого файла
            dirPath = QFileInfo(selectedFiles.first()->path()).absolutePath();
        }

        if (QDir(dirPath).exists()) {
            QUrl dirUrl = QUrl::fromLocalFile(dirPath);
            if (!QDesktopServices::openUrl(dirUrl)) {
                QMessageBox::warning(this, "Ошибка",
                    QString("Не удалось открыть директорию:\n%1").arg(dirPath));
            }
        } else {
            QMessageBox::warning(this, "Ошибка",
                QString("Директория не существует:\n%1").arg(dirPath));
        }
    }
}

void MainWindow::showFileProperties()
{
    auto selectedFiles = getSelectedFiles();

    if (selectedFiles.isEmpty()) return;

    if (selectedFiles.size() == 1) {
        // Показываем детальные свойства для одного файла
        auto file = selectedFiles.first();
        QFileInfo fileInfo(file->path());

        QString properties = QString(
            "<b>Свойства файла:</b><br>"
            "<br>"
            "<b>Имя:</b> %1<br>"
            "<b>Путь:</b> %2<br>"
            "<b>Размер:</b> %3<br>"
            "<b>Дата создания:</b> %4<br>"
            "<b>Дата изменения:</b> %5<br>"
            "<b>Дата последнего доступа:</b> %6<br>"
            "<b>Расширение:</b> %7<br>"
            "<b>Только для чтения:</b> %8<br>"
            "<b>Скрытый:</b> %9"
        ).arg(
            fileInfo.fileName(),
            fileInfo.absoluteFilePath(),
            formatSize(fileInfo.size()),
            fileInfo.birthTime().toString("dd.MM.yyyy HH:mm:ss"),
            fileInfo.lastModified().toString("dd.MM.yyyy HH:mm:ss"),
            fileInfo.lastRead().toString("dd.MM.yyyy HH:mm:ss"),
            fileInfo.suffix(),
            fileInfo.isReadable() && !fileInfo.isWritable() ? "Да" : "Нет",
            fileInfo.isHidden() ? "Да" : "Нет"
        );

        QMessageBox::information(this, "Свойства файла", properties);

    } else {
        // Показываем суммарную информацию для нескольких файлов
        qint64 totalSize = 0;
        QDateTime oldestDate = QDateTime::currentDateTime();
        QDateTime newestDate = QDateTime::fromSecsSinceEpoch(0);

        for (const auto &file : selectedFiles) {
            QFileInfo fileInfo(file->path());
            totalSize += fileInfo.size();

            if (fileInfo.lastModified() < oldestDate) {
                oldestDate = fileInfo.lastModified();
            }
            if (fileInfo.lastModified() > newestDate) {
                newestDate = fileInfo.lastModified();
            }
        }

        QString summary = QString(
            "<b>Свойства выбранных файлов (%1 шт.):</b><br>"
            "<br>"
            "<b>Общий размер:</b> %2<br>"
            "<b>Самый старый файл:</b> %3<br>"
            "<b>Самый новый файл:</b> %4"
        ).arg(
            QString::number(selectedFiles.size()),
            formatSize(totalSize),
            oldestDate.toString("dd.MM.yyyy HH:mm:ss"),
            newestDate.toString("dd.MM.yyyy HH:mm:ss")
        );

        QMessageBox::information(this, "Свойства файлов", summary);
    }
}

void MainWindow::copyFilePath()
{
    auto selectedFiles = getSelectedFiles();

    if (selectedFiles.isEmpty()) return;

    QStringList paths;
    for (const auto &file : selectedFiles) {
        paths.append(file->path());
    }

    QString clipboardText = paths.join("\n");
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipboardText);

    // Краткое уведомление
    ui->statusLabel->setText(
        QString("Скопировано %1 путь(ей)").arg(selectedFiles.size())
    );
}

void MainWindow::copyFileName()
{
    auto selectedFiles = getSelectedFiles();

    if (selectedFiles.isEmpty()) return;

    QStringList names;
    for (const auto &file : selectedFiles) {
        names.append(file->name());
    }

    QString clipboardText = names.join("\n");
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipboardText);

    // Краткое уведомление
    ui->statusLabel->setText(
        QString("Скопировано %1 имя(ён)").arg(selectedFiles.size())
    );
}
