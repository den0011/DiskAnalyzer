#include "sunburstwidget.h"
#include <QPainter>
#include <QToolTip>
#include <QtMath>
#include <QDebug>

SunburstWidget::SunburstWidget(QWidget *parent)
    : QWidget(parent)
    , m_rootItem(nullptr)
    , m_hoveredSegment(nullptr)
    , m_radius(0)
    , m_innerRadius(30)
{
    setMouseTracking(true);
    setMinimumSize(400, 400);
}

void SunburstWidget::setRootItem(std::shared_ptr<FileItem> root)
{
    m_rootItem = root;
    buildSegments();
    update();
}

void SunburstWidget::clear()
{
    m_rootItem = nullptr;
    m_segments.clear();
    m_hoveredSegment = nullptr;
    update();
}

void SunburstWidget::buildSegments()
{
    m_segments.clear();
    m_hoveredSegment = nullptr;

    if (!m_rootItem) return;

    // Вычисляем центр и радиус
    int side = qMin(width(), height());
    m_radius = side / 2.0 - 10;
    m_center = QPoint(width() / 2, height() / 2);

    qint64 totalSize = m_rootItem->totalSize();
    if (totalSize == 0) return;

    // Строим сегменты начиная с корня
    drawSegments(m_rootItem, 0, 360, 0, totalSize);

    qDebug() << "Построено сегментов:" << m_segments.size();
}

void SunburstWidget::drawSegments(std::shared_ptr<FileItem> item, qreal startAngle, qreal spanAngle, int level, qint64 totalSize)
{
    if (!item || spanAngle < 0.1 || level > 5) return; // Ограничиваем глубину

    // Добавляем текущий сегмент
    SunburstSegment segment;
    segment.item = item;
    segment.startAngle = startAngle;
    segment.spanAngle = spanAngle;
    segment.level = level;
    segment.color = getColorForItem(item, level);

    // Вычисляем прямоугольник для дуги
    qreal levelHeight = (m_radius - m_innerRadius) / 6.0; // 6 уровней максимум
    qreal innerRad = m_innerRadius + level * levelHeight;
    qreal outerRad = innerRad + levelHeight;

    segment.rect = QRectF(
        m_center.x() - outerRad,
        m_center.y() - outerRad,
        outerRad * 2,
        outerRad * 2
    );

    m_segments.append(segment);

    // Рекурсивно обрабатываем детей
    if (item->isDirectory() && !item->children().isEmpty()) {
        qreal currentAngle = startAngle;

        for (const auto &child : item->children()) {
            qint64 childSize = child->totalSize();
            if (childSize == 0) continue;

            qreal childSpan = (childSize / (qreal)item->totalSize()) * spanAngle;

            if (childSpan >= 0.5) { // Показываем только если сегмент достаточно большой
                drawSegments(child, currentAngle, childSpan, level + 1, childSize);
            }

            currentAngle += childSpan;
        }
    }
}

void SunburstWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Фон
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (m_segments.isEmpty()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Нет данных для отображения");
        return;
    }

    // Рисуем сегменты от самого глубокого уровня к корню
    for (int level = 5; level >= 0; --level) {
        for (const auto &segment : m_segments) {
            if (segment.level == level) {
                bool highlighted = (&segment == m_hoveredSegment);
                drawSegment(painter, segment, highlighted);
            }
        }
    }

    // Рисуем центральный круг
    painter.setBrush(QColor(50, 50, 50));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(m_center, m_innerRadius, m_innerRadius);

    // Текст в центре
    if (m_rootItem) {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(10);
        painter.setFont(font);

        QString text = m_rootItem->name();
        if (text.length() > 15) {
            text = text.left(12) + "...";
        }

        QRectF centerRect(
            m_center.x() - m_innerRadius,
            m_center.y() - m_innerRadius,
            m_innerRadius * 2,
            m_innerRadius * 2
        );

        painter.drawText(centerRect, Qt::AlignCenter | Qt::TextWordWrap, text);
    }
}

void SunburstWidget::drawSegment(QPainter &painter, const SunburstSegment &segment, bool highlighted)
{
    painter.save();

    // Цвет
    QColor color = segment.color;
    if (highlighted) {
        color = color.lighter(150);
    }

    painter.setBrush(color);
    painter.setPen(QPen(QColor(255, 255, 255, 100), 1));

    // Вычисляем радиусы для этого уровня
    qreal levelHeight = (m_radius - m_innerRadius) / 6.0;
    qreal innerRad = m_innerRadius + segment.level * levelHeight;
    qreal outerRad = innerRad + levelHeight;

    // Создаем путь для сегмента
    QPainterPath path;

    // Внешняя дуга
    QRectF outerRect(
        m_center.x() - outerRad,
        m_center.y() - outerRad,
        outerRad * 2,
        outerRad * 2
    );

    path.arcMoveTo(outerRect, segment.startAngle);
    path.arcTo(outerRect, segment.startAngle, segment.spanAngle);

    // Линия к внутреннему радиусу
    qreal endAngle = segment.startAngle + segment.spanAngle;
    qreal endRad = qDegreesToRadians(endAngle);
    QPointF innerEnd(
        m_center.x() + innerRad * qCos(endRad),
        m_center.y() - innerRad * qSin(endRad)
    );
    path.lineTo(innerEnd);

    // Внутренняя дуга (в обратном направлении)
    QRectF innerRect(
        m_center.x() - innerRad,
        m_center.y() - innerRad,
        innerRad * 2,
        innerRad * 2
    );
    path.arcTo(innerRect, endAngle, -segment.spanAngle);

    path.closeSubpath();

    painter.drawPath(path);

    painter.restore();
}

QColor SunburstWidget::getColorForItem(std::shared_ptr<FileItem> item, int level)
{
    if (!item) return QColor(100, 100, 100);

    // Базовые цвета в зависимости от типа
    QColor baseColor;

    if (item->isDirectory()) {
        // Директории - оттенки синего
        baseColor = QColor(70, 130, 180);
    } else {
        QString name = item->name().toLower();

        // Видео - красный
        if (name.endsWith(".mp4") || name.endsWith(".avi") || name.endsWith(".mkv") ||
            name.endsWith(".mov") || name.endsWith(".wmv")) {
            baseColor = QColor(220, 20, 60);
        }
        // Аудио - оранжевый
        else if (name.endsWith(".mp3") || name.endsWith(".wav") || name.endsWith(".flac") ||
                 name.endsWith(".m4a") || name.endsWith(".aac")) {
            baseColor = QColor(255, 140, 0);
        }
        // Изображения - зелёный
        else if (name.endsWith(".jpg") || name.endsWith(".jpeg") || name.endsWith(".png") ||
                 name.endsWith(".gif") || name.endsWith(".bmp")) {
            baseColor = QColor(50, 205, 50);
        }
        // Архивы - фиолетовый
        else if (name.endsWith(".zip") || name.endsWith(".rar") || name.endsWith(".7z") ||
                 name.endsWith(".tar") || name.endsWith(".gz")) {
            baseColor = QColor(153, 50, 204);
        }
        // Документы - голубой
        else if (name.endsWith(".pdf") || name.endsWith(".doc") || name.endsWith(".docx") ||
                 name.endsWith(".txt") || name.endsWith(".xlsx")) {
            baseColor = QColor(30, 144, 255);
        }
        // Остальное - серый
        else {
            baseColor = QColor(128, 128, 128);
        }
    }

    // Делаем цвет темнее с увеличением уровня
    int darkenFactor = 100 + level * 15;
    return baseColor.darker(darkenFactor);
}

QString SunburstWidget::formatSize(qint64 bytes) const
{
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;
    constexpr qint64 GB = MB * 1024;
    constexpr qint64 TB = GB * 1024;

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

SunburstSegment* SunburstWidget::findSegmentAt(const QPoint &pos)
{
    // Вычисляем расстояние от центра
    qreal dx = pos.x() - m_center.x();
    qreal dy = pos.y() - m_center.y();
    qreal distance = qSqrt(dx * dx + dy * dy);

    // Проверяем, попадаем ли в диапазон радиусов
    if (distance < m_innerRadius || distance > m_radius) {
        return nullptr;
    }

    // Вычисляем угол
    qreal angle = qRadiansToDegrees(qAtan2(-dy, dx));
    if (angle < 0) angle += 360;

    // Ищем сегмент
    for (auto &segment : m_segments) {
        qreal levelHeight = (m_radius - m_innerRadius) / 6.0;
        qreal innerRad = m_innerRadius + segment.level * levelHeight;
        qreal outerRad = innerRad + levelHeight;

        if (distance >= innerRad && distance <= outerRad) {
            qreal endAngle = segment.startAngle + segment.spanAngle;

            // Нормализуем углы для корректного сравнения
            qreal start = segment.startAngle;
            qreal end = endAngle;

            if (start > end) {
                // Сегмент пересекает 0 градусов
                if (angle >= start || angle <= end) {
                    return &segment;
                }
            } else {
                if (angle >= start && angle <= end) {
                    return &segment;
                }
            }
        }
    }

    return nullptr;
}

void SunburstWidget::mouseMoveEvent(QMouseEvent *event)
{
    SunburstSegment* segment = findSegmentAt(event->pos());

    if (segment != m_hoveredSegment) {
        m_hoveredSegment = segment;
        update();

        if (segment && segment->item) {
            QString tooltip = QString("%1\n%2\n%3")
                .arg(segment->item->name())
                .arg(formatSize(segment->item->totalSize()))
                .arg(segment->item->path());

            QToolTip::showText(event->globalPos(), tooltip, this);
        } else {
            QToolTip::hideText();
        }
    }
}

void SunburstWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        SunburstSegment* segment = findSegmentAt(event->pos());
        if (segment && segment->item) {
            emit itemClicked(segment->item);
        }
    }
}

void SunburstWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        SunburstSegment* segment = findSegmentAt(event->pos());
        if (segment && segment->item) {
            emit itemDoubleClicked(segment->item);
        }
    }
}

void SunburstWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    if (m_rootItem) {
        buildSegments();
    }
}
