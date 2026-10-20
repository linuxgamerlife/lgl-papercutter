#pragma once

#include "core/compositionstate.h"

#include <QImage>
#include <QPoint>
#include <QWidget>

namespace papercutter {

class CompositionCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit CompositionCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void setComposition(const CompositionState &composition);
    const CompositionState &composition() const;

signals:
    void compositionChanged(const papercutter::CompositionState &composition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QRectF canvasRect() const;
    QRectF imageRect(const QRectF &canvas) const;

    QImage m_image;
    CompositionState m_composition;
    QPoint m_dragOrigin;
    QPointF m_offsetAtDragStart;
};

} // namespace papercutter
