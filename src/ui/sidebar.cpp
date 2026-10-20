#include "ui/sidebar.h"

#include <gtkmm/adjustment.h>
#include <gtkmm/grid.h>

#include <cstdio>

namespace papercutter {

namespace {

Gtk::Label *sectionLabel(const char *text)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->add_css_class("section-label");
    label->set_xalign(0.0f);
    return label;
}

Gtk::Box *card(Gtk::Box &column, Gtk::Widget &heading)
{
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    box->add_css_class("pc-card");
    box->append(heading);
    column.append(*box);
    return box;
}

Gtk::Box *row(int spacing = 8)
{
    return Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, spacing);
}

void smallButton(Gtk::Button &button, const char *label, const char *tooltip)
{
    button.set_label(label);
    button.set_tooltip_text(tooltip);
    button.add_css_class("pc-small");
    button.set_valign(Gtk::Align::CENTER);
}

void configureScale(Gtk::Scale &scale)
{
    scale.set_draw_value(false);
    scale.set_digits(0);
    scale.set_round_digits(0);
    scale.set_hexpand(true);
}

Gtk::Label *fieldLabel(const char *text)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    label->add_css_class("muted");
    return label;
}

} // namespace

Sidebar::Sidebar()
    : width(Gtk::Adjustment::create(3440, 1, 32768, 1, 100), 1, 0)
    , height(Gtk::Adjustment::create(1440, 1, 32768, 1, 100), 1, 0)
    , zoom(Gtk::Adjustment::create(100, 5, 2000, 1, 10), Gtk::Orientation::HORIZONTAL)
    , quality(Gtk::Adjustment::create(95, 1, 100, 1, 5), Gtk::Orientation::HORIZONTAL)
    , m_column(Gtk::Orientation::VERTICAL, 12)
    , m_resolutionLabels(Gtk::StringList::create())
    , m_attributes(Gtk::Orientation::VERTICAL, 10)
{
    add_css_class("sidebar-scroller");
    set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    set_propagate_natural_width(true);
    set_size_request(300, -1);
    set_child(m_column);

    // --- target resolution
    auto *target = card(m_column, *sectionLabel("TARGET RESOLUTION"));
    resolution.set_model(m_resolutionLabels);
    resolution.set_hexpand(true);
    resolution.set_tooltip_text("Detected displays and saved presets");
    target->append(resolution);
    auto *dimensions = row();
    for (Gtk::SpinButton *spin : {&width, &height}) {
        spin->set_numeric(true);
        spin->set_width_chars(6);
        spin->set_hexpand(true);
        spin->add_css_class("mono");
    }
    width.set_tooltip_text("Output width in pixels");
    height.set_tooltip_text("Output height in pixels");
    auto *times = Gtk::make_managed<Gtk::Label>("×");
    times->add_css_class("muted");
    dimensions->append(width);
    dimensions->append(*times);
    dimensions->append(height);
    target->append(*dimensions);

    // --- composition
    auto *composition = card(m_column, *sectionLabel("COMPOSITION"));
    auto *zoomHead = row();
    auto *zoomLabel = fieldLabel("Zoom");
    zoomLabel->set_hexpand(true);
    m_zoomValue.add_css_class("mono");
    smallButton(resetZoom, "Reset", "Reset zoom to 100%");
    zoomHead->append(*zoomLabel);
    zoomHead->append(m_zoomValue);
    zoomHead->append(resetZoom);
    composition->append(*zoomHead);
    configureScale(zoom);
    zoom.set_tooltip_text("Zoom the current image. You can also scroll over the canvas.");
    composition->append(zoom);
    auto *scaleRow = row();
    auto *scaleLabel = fieldLabel("Effective scale");
    scaleLabel->set_hexpand(true);
    m_scaleValue.add_css_class("mono");
    m_scaleChip.add_css_class("pc-chip");
    m_scaleChip.set_valign(Gtk::Align::CENTER);
    scaleRow->append(*scaleLabel);
    scaleRow->append(m_scaleValue);
    scaleRow->append(m_scaleChip);
    composition->append(*scaleRow);
    m_scaleNote.add_css_class("muted");
    m_scaleNote.add_css_class("small");
    m_scaleNote.set_xalign(0.0f);
    m_scaleNote.set_wrap(true);
    composition->append(m_scaleNote);

    // --- output
    m_outputHeading.add_css_class("section-label");
    m_outputHeading.set_xalign(0.0f);
    auto *output = card(m_column, m_outputHeading);
    auto *qualityRow = row();
    configureScale(quality);
    quality.set_tooltip_text("JPEG and WebP only. Lower quality usually creates smaller files.");
    m_qualityValue.add_css_class("mono");
    m_qualityValue.set_width_chars(4);
    m_qualityValue.set_xalign(1.0f);
    smallButton(resetQuality, "Reset", "Reset output quality to 95%");
    qualityRow->append(quality);
    qualityRow->append(m_qualityValue);
    qualityRow->append(resetQuality);
    output->append(*qualityRow);
    auto *sizes = Gtk::make_managed<Gtk::Grid>();
    sizes->set_column_spacing(12);
    sizes->set_row_spacing(4);
    for (Gtk::Label *value : {&m_originalSize, &m_estimateSize}) {
        value->add_css_class("mono");
        value->set_xalign(1.0f);
        value->set_hexpand(true);
    }
    m_estimateLabel.set_xalign(0.0f);
    m_estimateLabel.add_css_class("muted");
    sizes->attach(*fieldLabel("Original file"), 0, 0);
    sizes->attach(m_originalSize, 1, 0);
    sizes->attach(m_estimateLabel, 0, 1);
    sizes->attach(m_estimateSize, 1, 1);
    output->append(*sizes);

    // --- image attributes
    m_attributesHeading.add_css_class("section-label");
    m_attributesHeading.set_xalign(0.0f);
    m_attributesHeading.set_text("IMAGE ATTRIBUTES");
    auto *attributes = card(m_column, m_attributesHeading);
    const struct {
        const char *label;
        Gtk::Entry *entry;
        const char *placeholder;
    } fields[] = {
        {"Description", &description, "Image description"},
        {"Artist", &artist, "Creator or photographer"},
        {"Copyright", &copyright, "Copyright notice"},
        {"Comment", &comment, "Comment"},
    };
    for (const auto &field : fields) {
        auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
        box->append(*fieldLabel(field.label));
        field.entry->set_placeholder_text(field.placeholder);
        box->append(*field.entry);
        m_attributes.append(*box);
    }
    attributes->append(m_attributes);

    // --- source
    m_source.add_css_class("mono");
    m_source.add_css_class("small");
    m_source.add_css_class("subtle");
    m_source.set_xalign(0.0f);
    m_source.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    m_source.set_margin_start(4);
    m_source.set_margin_end(4);
    m_column.append(m_source);

    quality.signal_value_changed().connect(
        [this] { m_qualityValue.set_text(std::to_string(outputQuality()) + "%"); });
    zoom.signal_value_changed().connect(
        [this] { m_zoomValue.set_text(std::to_string(zoomPercent()) + "%"); });
    m_qualityValue.set_text("95%");
    m_zoomValue.set_text("100%");
    setOutputFormat("OUTPUT QUALITY", false);
    setOutputEnabled(false);
    setScaleInfo(ScaleState::None, 0.0);
    setFileSizes({}, "Estimated output", {});
    setAttributesEnabled(false);
    setSourcePath({});
}

void Sidebar::setResolutionChoices(const std::vector<ResolutionChoice> &choices)
{
    m_choices = choices;
    std::vector<Glib::ustring> labels;
    for (const ResolutionChoice &choice : choices)
        labels.push_back(choice.label);
    m_resolutionLabels->splice(0, m_resolutionLabels->get_n_items(), labels);
}

void Sidebar::insertResolutionChoice(const int index, const ResolutionChoice &choice)
{
    m_choices.insert(m_choices.begin() + index, choice);
    m_resolutionLabels->splice(static_cast<guint>(index), 0, {choice.label});
}

void Sidebar::removeResolutionChoice(const int index)
{
    m_choices.erase(m_choices.begin() + index);
    m_resolutionLabels->remove(static_cast<guint>(index));
}

int Sidebar::findResolution(const Size &size) const
{
    for (std::size_t i = 0; i < m_choices.size(); ++i) {
        if (m_choices[i].size.isValid() && m_choices[i].size == size)
            return static_cast<int>(i);
    }
    return -1;
}

int Sidebar::selectedResolution() const
{
    const guint selected = resolution.get_selected();
    return selected == GTK_INVALID_LIST_POSITION || selected >= m_choices.size()
        ? -1 : static_cast<int>(selected);
}

void Sidebar::selectResolution(const int index)
{
    if (index >= 0 && index < static_cast<int>(m_choices.size()))
        resolution.set_selected(static_cast<guint>(index));
}

Size Sidebar::targetSize() const
{
    return {width.get_value_as_int(), height.get_value_as_int()};
}

void Sidebar::setTargetSize(const Size &size)
{
    width.set_value(size.width);
    height.set_value(size.height);
}

int Sidebar::zoomPercent() const
{
    return static_cast<int>(zoom.get_value() + 0.5);
}

void Sidebar::setZoomPercent(const int percent)
{
    zoom.set_value(percent);
}

void Sidebar::setScaleInfo(const ScaleState state, const double scale)
{
    for (const char *name : {"positive", "neutral", "negative"})
        m_scaleChip.remove_css_class(name);
    if (state == ScaleState::None) {
        m_scaleValue.set_text("—");
        m_scaleChip.set_visible(false);
        m_scaleNote.set_text("Add an image to see scale information.");
        return;
    }
    char text[32];
    std::snprintf(text, sizeof text, "%.2f×", scale);
    m_scaleValue.set_text(text);
    m_scaleChip.set_visible(true);
    switch (state) {
    case ScaleState::Sharp:
        m_scaleChip.set_text("SHARP");
        m_scaleChip.add_css_class("positive");
        m_scaleNote.set_text("The image is shown at or below its original size.");
        break;
    case ScaleState::Enlarged:
        m_scaleChip.set_text("ENLARGED");
        m_scaleChip.add_css_class("neutral");
        m_scaleNote.set_text("Enlargement may look soft.");
        break;
    case ScaleState::Soft:
    case ScaleState::None:
        m_scaleChip.set_text("SOFT");
        m_scaleChip.add_css_class("negative");
        m_scaleNote.set_text("Strong enlargement; the result will likely look soft.");
        break;
    }
}

void Sidebar::setOutputFormat(const std::string &heading, const bool qualityApplies)
{
    m_outputHeading.set_text(heading);
    quality.set_sensitive(qualityApplies);
    m_qualityValue.set_sensitive(qualityApplies);
    resetQuality.set_sensitive(qualityApplies);
}

void Sidebar::setOutputEnabled(const bool enabled)
{
    if (!enabled)
        setOutputFormat("OUTPUT QUALITY", false);
}

int Sidebar::outputQuality() const
{
    return static_cast<int>(quality.get_value() + 0.5);
}

void Sidebar::setOutputQuality(const int value)
{
    quality.set_value(value);
}

void Sidebar::setFileSizes(const std::string &original, const std::string &estimateLabel,
                           const std::string &estimate)
{
    m_originalSize.set_text(original.empty() ? "—" : original);
    m_estimateLabel.set_text(estimateLabel);
    m_estimateSize.set_text(estimate.empty() ? "—" : estimate);
}

void Sidebar::setAttributesAvailable(const bool available)
{
    m_attributesHeading.set_text(available ? "IMAGE ATTRIBUTES"
                                           : "IMAGE ATTRIBUTES (EXIV2 REQUIRED)");
    m_attributes.set_tooltip_text(available ? "" : "Install Exiv2 to view and edit image attributes.");
}

void Sidebar::setAttributesEnabled(const bool enabled)
{
    m_attributes.set_sensitive(enabled);
}

void Sidebar::setSourcePath(const std::string &path)
{
    m_source.set_text(path.empty() ? "No image selected" : path);
    m_source.set_tooltip_text(path);
}

} // namespace papercutter
