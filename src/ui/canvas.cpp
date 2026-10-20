#include "ui/canvas.h"

#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gesturedrag.h>
#include <gtkmm/snapshot.h>

#include <algorithm>

namespace papercutter {

namespace {

constexpr int canvasMargin = 24;

} // namespace

CanvasFrame::CanvasFrame()
    : Glib::ObjectBase("PapercutterCanvasFrame")
{
    add_css_class("canvas-frame");
    set_overflow(Gtk::Overflow::HIDDEN);
    m_placeholder.add_css_class("canvas-placeholder");
    m_placeholder.set_wrap(true);
    m_placeholder.set_justify(Gtk::Justification::CENTER);
    m_placeholder.set_parent(*this);
}

CanvasFrame::~CanvasFrame()
{
    m_placeholder.unparent();
}

void CanvasFrame::setImage(const Glib::RefPtr<Gdk::Texture> &texture, const Size &sourceSize)
{
    m_texture = texture;
    m_sourceSize = sourceSize;
    m_placeholder.set_visible(!m_texture);
    queue_draw();
}

void CanvasFrame::setComposition(const CompositionState &composition)
{
    m_composition = composition;
    queue_draw();
}

void CanvasFrame::setPlaceholder(const Glib::ustring &text)
{
    m_placeholder.set_text(text);
}

void CanvasFrame::measure_vfunc(Gtk::Orientation, int, int &minimum, int &natural,
                                int &minimum_baseline, int &natural_baseline) const
{
    minimum = 0;
    natural = 0;
    minimum_baseline = -1;
    natural_baseline = -1;
}

void CanvasFrame::size_allocate_vfunc(const int width, const int height, int)
{
    m_placeholder.size_allocate(Gtk::Allocation(0, 0, width, height), -1);
}

void CanvasFrame::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot> &snapshot)
{
    const double width = get_width();
    const double height = get_height();
    if (m_texture && m_sourceSize.isValid() && m_composition.targetSize.isValid()) {
        const double targetScale = m_composition.baseScaleFor(m_sourceSize) * m_composition.zoom;
        const double previewScale = width / m_composition.targetSize.width;
        const double imageWidth = m_sourceSize.width * targetScale * previewScale;
        const double imageHeight = m_sourceSize.height * targetScale * previewScale;
        const double travelX = std::max(0.0, (imageWidth - width) / 2.0);
        const double travelY = std::max(0.0, (imageHeight - height) / 2.0);
        const double centreX = width / 2.0 + m_composition.normalizedOffset.x * travelX;
        const double centreY = height / 2.0 + m_composition.normalizedOffset.y * travelY;

        snapshot->push_clip(Gdk::Graphene::Rect(0.0f, 0.0f, static_cast<float>(width),
                                                static_cast<float>(height)));
        snapshot->append_scaled_texture(
            m_texture, Gsk::ScalingFilter::TRILINEAR,
            Gdk::Graphene::Rect(static_cast<float>(centreX - imageWidth / 2.0),
                                static_cast<float>(centreY - imageHeight / 2.0),
                                static_cast<float>(imageWidth),
                                static_cast<float>(imageHeight)));
        snapshot->pop();
    }
    if (m_placeholder.get_visible())
        snapshot_child(m_placeholder, snapshot);
}

Canvas::Canvas()
    : Glib::ObjectBase("PapercutterCanvas")
{
    add_css_class("canvas-area");
    set_hexpand(true);
    set_vexpand(true);
    set_overflow(Gtk::Overflow::HIDDEN);
    m_frame.set_parent(*this);

    auto drag = Gtk::GestureDrag::create();
    drag->signal_drag_begin().connect(sigc::mem_fun(*this, &Canvas::dragBegin));
    drag->signal_drag_update().connect(sigc::mem_fun(*this, &Canvas::dragUpdate));
    drag->signal_drag_end().connect([this](double, double) { m_dragging = false; });
    add_controller(drag);

    auto scrollController = Gtk::EventControllerScroll::create();
    scrollController->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL
                                | Gtk::EventControllerScroll::Flags::DISCRETE);
    scrollController->signal_scroll().connect(sigc::mem_fun(*this, &Canvas::scroll), false);
    add_controller(scrollController);
}

Canvas::~Canvas()
{
    m_frame.unparent();
}

void Canvas::setImage(const Glib::RefPtr<Gdk::Texture> &texture, const Size &sourceSize)
{
    m_frame.setImage(texture, sourceSize);
}

void Canvas::clearImage(const Glib::ustring &placeholder)
{
    m_frame.setPlaceholder(placeholder);
    m_frame.setImage({}, {});
}

void Canvas::setComposition(const CompositionState &composition)
{
    const bool ratioChanged = composition.targetSize != m_frame.m_composition.targetSize;
    m_frame.setComposition(composition);
    if (ratioChanged)
        queue_allocate();
}

void Canvas::measure_vfunc(const Gtk::Orientation orientation, int, int &minimum, int &natural,
                           int &minimum_baseline, int &natural_baseline) const
{
    minimum = orientation == Gtk::Orientation::HORIZONTAL ? 480 : 270;
    natural = orientation == Gtk::Orientation::HORIZONTAL ? 800 : 450;
    minimum_baseline = -1;
    natural_baseline = -1;
}

void Canvas::size_allocate_vfunc(const int width, const int height, int)
{
    const double availableWidth = std::max(1, width - 2 * canvasMargin);
    const double availableHeight = std::max(1, height - 2 * canvasMargin);
    double fittedWidth = availableWidth;
    double fittedHeight = availableHeight;
    const Size target = m_frame.m_composition.targetSize;
    if (target.isValid()) {
        const double ratio = static_cast<double>(target.width) / target.height;
        fittedHeight = availableWidth / ratio;
        if (fittedHeight > availableHeight) {
            fittedWidth = availableHeight * ratio;
            fittedHeight = availableHeight;
        }
    }
    const int frameWidth = std::max(1, static_cast<int>(fittedWidth));
    const int frameHeight = std::max(1, static_cast<int>(fittedHeight));
    m_frame.size_allocate(Gtk::Allocation((width - frameWidth) / 2, (height - frameHeight) / 2,
                                          frameWidth, frameHeight),
                          -1);
}

void Canvas::dragBegin(const double x, const double y)
{
    const auto bounds = m_frame.compute_bounds(*this);
    m_dragging = m_frame.m_texture && bounds
        && bounds->contains_point(Gdk::Graphene::Point(static_cast<float>(x),
                                                      static_cast<float>(y)));
    m_offsetAtDragStart = m_frame.m_composition.normalizedOffset;
}

void Canvas::dragUpdate(const double offsetX, const double offsetY)
{
    if (!m_dragging)
        return;
    const double halfWidth = std::max(1.0, m_frame.get_width() / 2.0);
    const double halfHeight = std::max(1.0, m_frame.get_height() / 2.0);
    CompositionState &composition = m_frame.m_composition;
    composition.normalizedOffset.x =
        std::clamp(m_offsetAtDragStart.x + offsetX / halfWidth, -1.0, 1.0);
    composition.normalizedOffset.y =
        std::clamp(m_offsetAtDragStart.y + offsetY / halfHeight, -1.0, 1.0);
    m_frame.queue_draw();
    m_compositionChanged.emit(composition);
}

bool Canvas::scroll(double, const double dy)
{
    if (!m_frame.m_texture || dy == 0.0)
        return false;
    const double step = dy < 0.0 ? 1.05 : 1.0 / 1.05;
    CompositionState &composition = m_frame.m_composition;
    composition.zoom = std::clamp(composition.zoom * step, 0.05, 20.0);
    m_frame.queue_draw();
    m_compositionChanged.emit(composition);
    return true;
}

} // namespace papercutter
