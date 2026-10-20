#pragma once

#include "core/composition.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stringlist.h>

#include <string>
#include <vector>

namespace papercutter {

// An entry in the target resolution menu. Transient entries ("Mixed resolutions",
// "Staged — …") describe the current selection and are rebuilt on every selection change.
struct ResolutionChoice {
    std::string label;
    Size size;
    bool transient{false};
};

enum class ScaleState { None, Sharp, Enlarged, Soft };

// The settings column: target resolution, composition, output and image attribute cards.
// The window owns the behaviour; this class builds the widgets and formats read-outs.
class Sidebar final : public Gtk::ScrolledWindow {
public:
    Sidebar();

    // Target resolution
    void setResolutionChoices(const std::vector<ResolutionChoice> &choices);
    const std::vector<ResolutionChoice> &resolutionChoices() const { return m_choices; }
    void insertResolutionChoice(int index, const ResolutionChoice &choice);
    void removeResolutionChoice(int index);
    // First choice with this size (any kind), or -1.
    int findResolution(const Size &size) const;
    int selectedResolution() const;
    void selectResolution(int index);
    Size targetSize() const;
    void setTargetSize(const Size &size);

    // Composition
    int zoomPercent() const;
    void setZoomPercent(int percent);
    void setScaleInfo(ScaleState state, double scale);

    // Output
    void setOutputFormat(const std::string &heading, bool qualityApplies);
    void setOutputEnabled(bool enabled);
    int outputQuality() const;
    void setOutputQuality(int quality);
    void setFileSizes(const std::string &original, const std::string &estimateLabel,
                      const std::string &estimate);

    // Attributes
    void setAttributesAvailable(bool available);
    void setAttributesEnabled(bool enabled);

    void setSourcePath(const std::string &path);

    Gtk::DropDown resolution;
    Gtk::SpinButton width;
    Gtk::SpinButton height;
    Gtk::Scale zoom;
    Gtk::Button resetZoom;
    Gtk::Scale quality;
    Gtk::Button resetQuality;
    Gtk::Entry description;
    Gtk::Entry artist;
    Gtk::Entry copyright;
    Gtk::Entry comment;

private:
    Gtk::Box m_column;
    Glib::RefPtr<Gtk::StringList> m_resolutionLabels;
    std::vector<ResolutionChoice> m_choices;
    Gtk::Label m_zoomValue;
    Gtk::Label m_scaleValue;
    Gtk::Label m_scaleChip;
    Gtk::Label m_scaleNote;
    Gtk::Label m_outputHeading;
    Gtk::Label m_qualityValue;
    Gtk::Label m_originalSize;
    Gtk::Label m_estimateLabel;
    Gtk::Label m_estimateSize;
    Gtk::Label m_attributesHeading;
    Gtk::Box m_attributes;
    Gtk::Label m_source;
};

} // namespace papercutter
