#include "ui/helpwindow.h"

#include <giomm/resource.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>

#include <sstream>
#include <vector>

namespace papercutter {

namespace {

using Tags = std::vector<Glib::RefPtr<Gtk::TextTag>>;

Glib::RefPtr<Gtk::TextTag> tag(const Glib::RefPtr<Gtk::TextBuffer> &buffer, const char *name)
{
    return buffer->get_tag_table()->lookup(name);
}

// Inserts text, styling **bold** and `code` spans on top of the base tags.
void insertInline(const Glib::RefPtr<Gtk::TextBuffer> &buffer, const std::string &text,
                  const Tags &base)
{
    bool bold = false;
    bool code = false;
    std::string run;
    const auto flush = [&] {
        if (run.empty())
            return;
        Tags tags = base;
        if (bold)
            tags.push_back(tag(buffer, "bold"));
        if (code)
            tags.push_back(tag(buffer, "code"));
        if (tags.empty())
            buffer->insert(buffer->end(), run);
        else
            buffer->insert_with_tags(buffer->end(), run, tags);
        run.clear();
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!code && text.compare(i, 2, "**") == 0) {
            flush();
            bold = !bold;
            ++i;
        } else if (text[i] == '`') {
            flush();
            code = !code;
        } else {
            run += text[i];
        }
    }
    flush();
}

void renderMarkdown(const Glib::RefPtr<Gtk::TextBuffer> &buffer, const std::string &markdown)
{
    std::istringstream lines(markdown);
    std::string line;
    std::string paragraph;
    const auto endParagraph = [&] {
        if (paragraph.empty())
            return;
        insertInline(buffer, paragraph, {tag(buffer, "paragraph")});
        buffer->insert(buffer->end(), "\n");
        paragraph.clear();
    };
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const struct {
            const char *prefix;
            const char *tag;
        } headings[] = {{"### ", "h3"}, {"## ", "h2"}, {"# ", "h1"}};
        bool handled = false;
        for (const auto &heading : headings) {
            if (line.rfind(heading.prefix, 0) == 0) {
                endParagraph();
                insertInline(buffer, line.substr(std::string(heading.prefix).size()),
                             {tag(buffer, heading.tag)});
                buffer->insert(buffer->end(), "\n");
                handled = true;
                break;
            }
        }
        if (handled)
            continue;
        if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
            endParagraph();
            insertInline(buffer, "•  " + line.substr(2), {tag(buffer, "bullet")});
            buffer->insert(buffer->end(), "\n");
        } else if (line.find_first_not_of(" \t") == std::string::npos) {
            endParagraph();
        } else {
            if (!paragraph.empty())
                paragraph += ' ';
            paragraph += line;
        }
    }
    endParagraph();
}

} // namespace

HelpWindow::HelpWindow(Gtk::Window &parent)
    : m_buffer(Gtk::TextBuffer::create())
{
    set_title("LGL Papercutter Help");
    set_transient_for(parent);
    set_default_size(780, 680);
    set_hide_on_close(true);

    auto *header = Gtk::make_managed<Gtk::HeaderBar>();
    auto *title = Gtk::make_managed<Gtk::Label>("HELP");
    title->add_css_class("wordmark");
    header->set_title_widget(*title);
    set_titlebar(*header);

    auto h1 = m_buffer->create_tag("h1");
    h1->property_scale() = 1.6;
    h1->property_weight() = Pango::Weight::SEMIBOLD;
    h1->property_pixels_below_lines() = 10;
    auto h2 = m_buffer->create_tag("h2");
    h2->property_scale() = 1.2;
    h2->property_weight() = Pango::Weight::SEMIBOLD;
    h2->property_pixels_above_lines() = 18;
    h2->property_pixels_below_lines() = 6;
    auto h3 = m_buffer->create_tag("h3");
    h3->property_weight() = Pango::Weight::SEMIBOLD;
    h3->property_pixels_above_lines() = 12;
    h3->property_pixels_below_lines() = 4;
    auto paragraph = m_buffer->create_tag("paragraph");
    paragraph->property_pixels_below_lines() = 8;
    auto bullet = m_buffer->create_tag("bullet");
    bullet->property_left_margin() = 18;
    bullet->property_indent() = -14;
    bullet->property_pixels_below_lines() = 4;
    auto bold = m_buffer->create_tag("bold");
    bold->property_weight() = Pango::Weight::BOLD;
    m_buffer->create_tag("code");
    setMonoFamily("Adwaita Mono");

    try {
        const auto bytes = Gio::Resource::lookup_data_global(
            "/com/linuxgamerlife/papercutter/HELP.md");
        gsize size = 0;
        const auto *data = static_cast<const char *>(bytes->get_data(size));
        renderMarkdown(m_buffer, std::string(data, size));
    } catch (const Glib::Error &) {
        m_buffer->set_text("The LGL Papercutter help file could not be opened.");
    }

    m_view.set_buffer(m_buffer);
    m_view.set_editable(false);
    m_view.set_cursor_visible(false);
    m_view.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_view.set_left_margin(28);
    m_view.set_right_margin(28);
    m_view.set_top_margin(24);
    m_view.set_bottom_margin(24);
    m_view.add_css_class("help-view");

    auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_child(m_view);
    set_child(*scroller);
}

void HelpWindow::setMonoFamily(const std::string &family)
{
    for (const char *name : {"h1", "h2", "h3", "code"}) {
        if (auto found = m_buffer->get_tag_table()->lookup(name))
            found->property_family() = family;
    }
}

} // namespace papercutter
