#pragma once

#include "core/composition.h"

#include <gdkmm/texture.h>
#include <gtkmm/label.h>
#include <gtkmm/widget.h>

namespace papercutter {

// The target-ratio frame: black, clipped, with the accent outline from style.css
// (.canvas-frame). Draws the image at the composition's offset and zoom.
class CanvasFrame final : public Gtk::Widget {
public:
    CanvasFrame();
    ~CanvasFrame() override;

    void setImage(const Glib::RefPtr<Gdk::Texture> &texture, const Size &sourceSize);
    void setComposition(const CompositionState &composition);
    void setPlaceholder(const Glib::ustring &text);

protected:
    void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot> &snapshot) override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size, int &minimum, int &natural,
                       int &minimum_baseline, int &natural_baseline) const override;
    void size_allocate_vfunc(int width, int height, int baseline) override;

private:
    friend class Canvas;

    Glib::RefPtr<Gdk::Texture> m_texture;
    Size m_sourceSize;
    CompositionState m_composition;
    Gtk::Label m_placeholder;
};

// Port of src/ui/compositioncanvas.cpp: centres the frame on a sunken background, drags
// the normalised offset and zooms with the scroll wheel (×1.05 steps, 0.05–20).
class Canvas final : public Gtk::Widget {
public:
    Canvas();
    ~Canvas() override;

    void setImage(const Glib::RefPtr<Gdk::Texture> &texture, const Size &sourceSize);
    void clearImage(const Glib::ustring &placeholder);
    void setComposition(const CompositionState &composition);
    const CompositionState &composition() const { return m_frame.m_composition; }

    sigc::signal<void(const CompositionState &)> &signalCompositionChanged()
    {
        return m_compositionChanged;
    }

protected:
    void measure_vfunc(Gtk::Orientation orientation, int for_size, int &minimum, int &natural,
                       int &minimum_baseline, int &natural_baseline) const override;
    void size_allocate_vfunc(int width, int height, int baseline) override;

private:
    void dragBegin(double x, double y);
    void dragUpdate(double offsetX, double offsetY);
    bool scroll(double dx, double dy);

    CanvasFrame m_frame;
    bool m_dragging{false};
    PointF m_offsetAtDragStart;
    sigc::signal<void(const CompositionState &)> m_compositionChanged;
};

} // namespace papercutter
