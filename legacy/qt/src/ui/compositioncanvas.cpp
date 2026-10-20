#include "ui/compositioncanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace papercutter {

CompositionCanvas::CompositionCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(480, 270);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
}

void CompositionCanvas::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void CompositionCanvas::setComposition(const CompositionState &composition)
{
    m_composition = composition;
    update();
}

const CompositionState &CompositionCanvas::composition() const
{
    return m_composition;
}

QRectF CompositionCanvas::canvasRect() const
{
    const QRectF available = rect().adjusted(24, 24, -24, -24);
    if (!m_composition.targetSize.isValid())
        return available;

    const double ratio = static_cast<double>(m_composition.targetSize.width())
        / m_composition.targetSize.height();
    QSizeF fitted(available.width(), available.width() / ratio);
    if (fitted.height() > available.height())
        fitted = QSizeF(available.height() * ratio, available.height());
    return QRectF(QPointF(available.center().x() - fitted.width() / 2.0,
                          available.center().y() - fitted.height() / 2.0), fitted);
}

QRectF CompositionCanvas::imageRect(const QRectF &canvas) const
{
    if (m_image.isNull())
        return {};

    const double targetScale = m_composition.baseScaleFor(m_image.size())
        * m_composition.zoom;
    const double previewScale = canvas.width() / m_composition.targetSize.width();
    const QSizeF size(m_image.width() * targetScale * previewScale,
                      m_image.height() * targetScale * previewScale);
    const QPointF travel(std::max(0.0, (size.width() - canvas.width()) / 2.0),
                         std::max(0.0, (size.height() - canvas.height()) / 2.0));
    const QPointF centre = canvas.center()
        + QPointF(m_composition.normalizedOffset.x() * travel.x(),
                  m_composition.normalizedOffset.y() * travel.y());
    return QRectF(QPointF(centre.x() - size.width() / 2.0,
                          centre.y() - size.height() / 2.0), size);
}

void CompositionCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), palette().brush(QPalette::AlternateBase));

    const QRectF canvas = canvasRect();
    painter.save();
    painter.setClipRect(canvas);
    painter.fillRect(canvas, Qt::black);

    if (m_image.isNull()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(canvas, Qt::AlignCenter,
                         QStringLiteral("Add an image to begin composing"));
    } else {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(imageRect(canvas), m_image);
    }
    painter.restore();

    QPen border(palette().color(QPalette::Highlight));
    border.setWidth(2);
    painter.setPen(border);
    painter.drawRect(canvas);
}

void CompositionCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && canvasRect().contains(event->position())) {
        m_dragOrigin = event->position().toPoint();
        m_offsetAtDragStart = m_composition.normalizedOffset;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void CompositionCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_image.isNull()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint delta = event->position().toPoint() - m_dragOrigin;
    const QRectF canvas = canvasRect();
    const QPointF normalized(delta.x() / std::max(1.0, canvas.width() / 2.0),
                             delta.y() / std::max(1.0, canvas.height() / 2.0));
    m_composition.normalizedOffset.setX(
        std::clamp(m_offsetAtDragStart.x() + normalized.x(), -1.0, 1.0));
    m_composition.normalizedOffset.setY(
        std::clamp(m_offsetAtDragStart.y() + normalized.y(), -1.0, 1.0));
    update();
    emit compositionChanged(m_composition);
    event->accept();
}

void CompositionCanvas::wheelEvent(QWheelEvent *event)
{
    const double step = event->angleDelta().y() > 0 ? 1.05 : 1.0 / 1.05;
    m_composition.zoom = std::clamp(m_composition.zoom * step, 0.05, 20.0);
    update();
    emit compositionChanged(m_composition);
    event->accept();
}

} // namespace papercutter
