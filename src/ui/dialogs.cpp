#include "ui/dialogs.h"

#include <giomm/liststore.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/columnview.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/noselection.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/signallistitemfactory.h>

#include <gdk/gdkkeysyms.h>

namespace papercutter {

namespace {

class MappingRow final : public Glib::Object {
public:
    static Glib::RefPtr<MappingRow> create(std::string source, std::string sourcePath,
                                           std::string destination)
    {
        return Glib::make_refptr_for_instance<MappingRow>(
            new MappingRow(std::move(source), std::move(sourcePath), std::move(destination)));
    }

    std::string source;
    std::string sourcePath;
    std::string destination;

private:
    MappingRow(std::string source, std::string sourcePath, std::string destination)
        : Glib::ObjectBase(typeid(MappingRow))
        , source(std::move(source))
        , sourcePath(std::move(sourcePath))
        , destination(std::move(destination))
    {
    }
};

Glib::RefPtr<Gtk::ColumnViewColumn> textColumn(const char *title, const bool destination)
{
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect([destination](const Glib::RefPtr<Gtk::ListItem> &item) {
        auto *label = Gtk::make_managed<Gtk::Label>();
        label->set_xalign(0.0f);
        label->set_ellipsize(Pango::EllipsizeMode::MIDDLE);
        if (destination)
            label->add_css_class("mono");
        item->set_child(*label);
    });
    factory->signal_bind().connect([destination](const Glib::RefPtr<Gtk::ListItem> &item) {
        auto *label = dynamic_cast<Gtk::Label *>(item->get_child());
        auto row = std::dynamic_pointer_cast<MappingRow>(item->get_item());
        if (!label || !row)
            return;
        label->set_text(destination ? row->destination : row->source);
        label->set_tooltip_text(destination ? row->destination : row->sourcePath);
    });
    auto column = Gtk::ColumnViewColumn::create(title, factory);
    column->set_resizable(true);
    if (destination)
        column->set_expand(true);
    else
        column->set_fixed_width(300);
    return column;
}

// Modal preview of an export; deletes itself after answering once.
class MappingDialog final : public Gtk::Window {
public:
    MappingDialog(Gtk::Window &parent, const std::vector<ProcessingRequest> &requests,
                  std::function<void(bool)> done)
        : m_done(std::move(done))
    {
        set_title("Confirm Save As");
        set_transient_for(parent);
        set_modal(true);
        set_default_size(820, 360);
        set_hide_on_close(true);

        auto *header = Gtk::make_managed<Gtk::HeaderBar>();
        auto *title = Gtk::make_managed<Gtk::Label>("CONFIRM SAVE AS");
        title->add_css_class("wordmark");
        header->set_title_widget(*title);
        set_titlebar(*header);

        auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
        body->add_css_class("dialog-body");
        auto *summary = Gtk::make_managed<Gtk::Label>(
            requests.size() == 1
                ? std::string("Review the file that will be created:")
                : "Review the " + std::to_string(requests.size())
                    + " files that will be created:");
        summary->set_xalign(0.0f);
        body->append(*summary);

        auto store = Gio::ListStore<MappingRow>::create();
        for (const ProcessingRequest &request : requests)
            store->append(MappingRow::create(Glib::path_get_basename(request.job.sourcePath),
                                             request.job.sourcePath,
                                             request.destinationPath));
        auto *view = Gtk::make_managed<Gtk::ColumnView>(Gtk::NoSelection::create(store));
        view->append_column(textColumn("Source", false));
        view->append_column(textColumn("Destination", true));
        view->set_show_row_separators(true);
        view->add_css_class("mapping-view");
        auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
        scroller->set_child(*view);
        scroller->set_vexpand(true);
        scroller->set_has_frame(true);
        body->append(*scroller);

        auto *buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        buttons->set_halign(Gtk::Align::END);
        auto *cancel = Gtk::make_managed<Gtk::Button>("Cancel");
        auto *save = Gtk::make_managed<Gtk::Button>(
            requests.size() == 1 ? "Save File" : "Save Files");
        save->add_css_class("pc-primary");
        save->add_css_class("suggested-action");
        cancel->signal_clicked().connect([this] { finish(false); });
        save->signal_clicked().connect([this] { finish(true); });
        buttons->append(*cancel);
        buttons->append(*save);
        body->append(*buttons);
        set_child(*body);
        set_default_widget(*save);

        auto keys = Gtk::EventControllerKey::create();
        keys->signal_key_pressed().connect(
            [this](guint keyval, guint, Gdk::ModifierType) {
                if (keyval != GDK_KEY_Escape)
                    return false;
                finish(false);
                return true;
            },
            false);
        add_controller(keys);
        signal_close_request().connect(
            [this] {
                finish(false);
                return true;
            },
            false);
        save->grab_focus();
    }

private:
    void finish(const bool accepted)
    {
        if (m_answered)
            return;
        m_answered = true;
        set_visible(false);
        auto done = std::move(m_done);
        Glib::signal_idle().connect_once([this] { delete this; });
        if (done)
            done(accepted);
    }

    std::function<void(bool)> m_done;
    bool m_answered{false};
};

} // namespace

void showMessage(Gtk::Window &parent, const std::string &heading, const std::string &detail)
{
    auto dialog = Gtk::AlertDialog::create(heading);
    dialog->set_detail(detail);
    dialog->set_buttons({"Close"});
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);
    dialog->show(parent);
}

void askChoice(Gtk::Window &parent, const std::string &heading, const std::string &detail,
               const std::vector<Glib::ustring> &buttons, const int cancelButton,
               const int defaultButton, std::function<void(int)> done)
{
    auto dialog = Gtk::AlertDialog::create(heading);
    dialog->set_detail(detail);
    dialog->set_buttons(buttons);
    dialog->set_cancel_button(cancelButton);
    dialog->set_default_button(defaultButton);
    dialog->choose(parent, [dialog, cancelButton, done = std::move(done)](
                               Glib::RefPtr<Gio::AsyncResult> &result) {
        int answer = cancelButton;
        try {
            answer = dialog->choose_finish(result);
        } catch (const Glib::Error &) {
        }
        done(answer);
    });
}

void confirmExportMappings(Gtk::Window &parent, const std::vector<ProcessingRequest> &requests,
                           std::function<void(bool)> done)
{
    auto *dialog = new MappingDialog(parent, requests, std::move(done));
    dialog->present();
}

} // namespace papercutter
