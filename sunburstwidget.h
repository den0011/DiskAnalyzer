#ifndef SUNBURSTWIDGET_H
#define SUNBURSTWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <memory>
#include "fileitem.h"

// Структура для хранения информации о сегменте
struct SunburstSegment {
    std::shared_ptr<FileItem> item;
    qreal startAngle;
    qreal spanAngle;
    int level;
    QRectF rect;
    QColor color;
};

class SunburstWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SunburstWidget(QWidget *parent = nullptr);

    void setRootItem(std::shared_ptr<FileItem> root);
    void clear();

signals:
    void itemClicked(std::shared_ptr<FileItem> item);
    void itemDoubleClicked(std::shared_ptr<FileItem> item);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildSegments();
    void drawSegment(QPainter &painter, const SunburstSegment &segment, bool highlighted);
    void drawSegments(std::shared_ptr<FileItem> item, qreal startAngle, qreal spanAngle, int level, qint64 totalSize);

    QColor getColorForItem(std::shared_ptr<FileItem> item, int level);
    QString formatSize(qint64 bytes) const;
    SunburstSegment* findSegmentAt(const QPoint &pos);

    std::shared_ptr<FileItem> m_rootItem;
    QList<SunburstSegment> m_segments;
    SunburstSegment* m_hoveredSegment;
    QPoint m_center;
    qreal m_radius;
    qreal m_innerRadius;
};

#endif // SUNBURSTWIDGET_H
