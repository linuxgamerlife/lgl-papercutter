#include "ui/window.h"

#include "core/imageinfo.h"
#include "core/processor.h"
#include "ui/dialogs.h"
#include "ui/helpwindow.h"
#include "ui/themewatcher.h"

#include <gdk/gdkkeysyms.h>
#include <gdkmm/display.h>
#include <gdkmm/monitor.h>
#include <gdkmm/surface.h>
#include <giomm/file.h>
#include <giomm/liststore.h>
#include <giomm/menu.h>
#include <glibmm/checksum.h>
#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/button.h>
#include <gtkmm/droptarget.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/image.h>
#include <gtkmm/overlay.h>
#include <gtkmm/paned.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <unistd.h>

namespace papercutter {

namespace {

constexpr int previewMaxDimension = 4096;
constexpr int thumbnailMaxDimension = 112;
constexpr std::size_t previewCacheSize = 6;
constexpr unsigned int estimateDelayMs = 350;

std::string formatFileSize(const long long bytes)
{
    char text[32];
    if (bytes < 0)
        return "Unavailable";
    if (bytes < 1024)
        std::snprintf(text, sizeof text, "%lld B", bytes);
    else if (bytes < 1024 * 1024)
        std::snprintf(text, sizeof text, "%.1f KiB", bytes / 1024.0);
    else if (bytes < 1024LL * 1024 * 1024)
        std::snprintf(text, sizeof text, "%.2f MiB", bytes / (1024.0 * 1024));
    else
        std::snprintf(text, sizeof text, "%.2f GiB", bytes / (1024.0 * 1024 * 1024));
    return text;
}

std::string outputFormatName(const std::string &suffix)
{
    if (suffix == "jpg" || suffix == "jpeg")
        return "JPEG";
    if (suffix == "webp")
        return "WebP";
    if (suffix == "png")
        return "PNG";
    std::string upper = suffix;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper;
}

std::string uppercase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string sizeText(const Size &size)
{
    return std::to_string(size.width) + " × " + std::to_string(size.height);
}

long long fileSize(const std::string &path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    return error ? -1 : static_cast<long long>(size);
}

std::string absolutePath(const std::string &path)
{
    return Gio::File::create_for_path(path)->get_path();
}

std::string fileSha256(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    Glib::Checksum checksum(Glib::Checksum::Type::SHA256);
    char buffer[1 << 16];
    while (file) {
        file.read(buffer, sizeof buffer);
        const auto read = file.gcount();
        if (read > 0)
            checksum.update(reinterpret_cast<const guchar *>(buffer), static_cast<gsize>(read));
    }
    if (file.bad())
        return {};
    return checksum.get_string();
}

Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> imageFilters()
{
    auto images = Gtk::FileFilter::create();
    images->set_name("Images (*.jpg *.jpeg *.png *.webp)");
    for (const char *suffix : {"jpg", "jpeg", "png", "webp"})
        images->add_suffix(suffix);
    auto filters = Gio::ListStore<Gtk::FileFilter>::create();
    filters->append(images);
    return filters;
}

bool isAlt(const guint keyval)
{
    return keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R;
}

Glib::RefPtr<Gdk::Texture> appIcon()
{
    static const auto icon = Gdk::Texture::create_from_resource(
        "/com/linuxgamerlife/papercutter/lgl-papercutter.png");
    return icon;
}

} // namespace

Window::Window(const Glib::RefPtr<Gtk::Application> &application)
    : m_content(Gtk::Orientation::VERTICAL)
    , m_noticeBox(Gtk::Orientation::HORIZONTAL)
    , m_export(*this, m_settings,
               [this](const std::string &text, NoticeKind kind, unsigned int timeoutMs) {
                   notify(text, kind, timeoutMs);
               })
{
    set_application(application);
    set_title("LGL Papercutter");
    set_icon_name("lgl-papercutter");
    set_default_size(1280, 800);

    m_theme = std::make_unique<ThemeWatcher>(*this);
    buildHeader();
    buildBody();
    buildActions(application);
    applyTitleBar(m_settings.systemTitleBar());
    // The open menu is a separate surface that receives its own key events.
    watchAltTap(*this);
    watchAltTap(*m_menuButton->get_popover());

    m_sidebar.setAttributesAvailable(!ImageProcessor::metadataExecutable().empty());
    m_canvas.clearImage("Add an image to begin composing");
    populateResolutionTargets(true);
    get_display()->get_monitors()->signal_items_changed().connect(
        [this](guint, guint, guint) { populateResolutionTargets(m_queue.size() == 0); });

    m_queue.signalCurrentChanged().connect(sigc::mem_fun(*this, &Window::loadCurrentJob));
    m_queue.signalSelectionChanged().connect(
        sigc::mem_fun(*this, &Window::syncResolutionControlsToSelection));
    m_queue.store()->signal_items_changed().connect(
        [this](guint, guint, guint) { updateActions(); });

    m_sidebar.width.signal_value_changed().connect(
        sigc::mem_fun(*this, &Window::applyTargetResolutionToSelection));
    m_sidebar.height.signal_value_changed().connect(
        sigc::mem_fun(*this, &Window::applyTargetResolutionToSelection));
    m_sidebar.zoom.signal_value_changed().connect(
        sigc::mem_fun(*this, &Window::updateCurrentZoomFromControl));
    m_sidebar.quality.signal_value_changed().connect(
        sigc::mem_fun(*this, &Window::updateCurrentOutputQuality));
    m_sidebar.resetQuality.signal_clicked().connect([this] { m_sidebar.setOutputQuality(95); });
    m_sidebar.resetZoom.signal_clicked().connect([this] { m_sidebar.setZoomPercent(100); });
    for (Gtk::Entry *field : {&m_sidebar.description, &m_sidebar.artist, &m_sidebar.copyright,
                              &m_sidebar.comment})
        field->signal_changed().connect(
            sigc::mem_fun(*this, &Window::updateCurrentMetadataFromControls));
    m_sidebar.resolution.property_selected().signal_changed().connect([this] {
        if (m_updatingControls)
            return;
        const int index = m_sidebar.selectedResolution();
        if (index < 0)
            return;
        const Size size = m_sidebar.resolutionChoices()[static_cast<std::size_t>(index)].size;
        if (!size.isValid())
            return;
        m_updatingControls = true;
        m_sidebar.setTargetSize(size);
        m_updatingControls = false;
        applyTargetResolutionToSelection();
    });
    m_canvas.signalCompositionChanged().connect([this](const CompositionState &state) {
        auto item = currentItem();
        if (!item)
            return;
        item->job.composition = state;
        m_updatingControls = true;
        m_sidebar.setZoomPercent(static_cast<int>(std::lround(state.zoom * 100.0)));
        m_updatingControls = false;
        updateQualityLabel();
    });
    m_theme->signalChanged().connect([this] {
        if (m_help)
            m_help->setMonoFamily(m_theme->monoFamily());
    });

    auto drop = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY);
    drop->signal_drop().connect(
        [this](const Glib::ValueBase &value, double, double) {
            if (!G_VALUE_HOLDS(value.gobj(), GDK_TYPE_FILE_LIST))
                return false;
            auto *list = static_cast<GdkFileList *>(g_value_get_boxed(value.gobj()));
            std::vector<std::string> paths;
            GSList *files = list ? gdk_file_list_get_files(list) : nullptr;
            for (GSList *node = files; node; node = node->next) {
                if (gchar *path = g_file_get_path(G_FILE(node->data))) {
                    paths.emplace_back(path);
                    g_free(path);
                }
            }
            g_slist_free(files);
            enqueuePaths(paths);
            return true;
        },
        false);
    add_controller(drop);

    updateActions();
    updateQualityLabel();
}

Window::~Window() = default;

void Window::on_realize()
{
    Gtk::ApplicationWindow::on_realize();
    // GTK 4 has no primary monitor; default to the display the window opens on.
    get_surface()->signal_enter_monitor().connect([this](const Glib::RefPtr<Gdk::Monitor> &) {
        const bool selectDefault = !m_defaultMonitorApplied && m_queue.size() == 0;
        m_defaultMonitorApplied = true;
        populateResolutionTargets(selectDefault);
    });
}

void Window::buildHeader()
{
    auto *brand = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    auto *icon = Gtk::make_managed<Gtk::Image>(appIcon());
    icon->set_pixel_size(28);
    icon->add_css_class("brand-icon");
    brand->append(*icon);
    auto *wordmark = Gtk::make_managed<Gtk::Label>("LGL PAPERCUTTER");
    wordmark->add_css_class("wordmark");
    brand->append(*wordmark);
    brand->set_margin_end(12);
    m_header.pack_start(*brand);
    // The wordmark is the title; keep the centre empty.
    m_header.set_title_widget(*Gtk::make_managed<Gtk::Box>());

    const struct {
        const char *label;
        const char *action;
        const char *tooltip;
    } buttons[] = {
        {"Add Images", "win.add-images", "Add images to the queue (Ctrl+O)"},
        {"Remove", "win.remove-selected",
         "Remove the selected entries from the queue (Delete). Files are not deleted."},
        {"Remove Duplicates", "win.remove-duplicates",
         "Remove later queue entries whose image content is identical"},
    };
    for (const auto &spec : buttons) {
        auto *button = Gtk::make_managed<Gtk::Button>(spec.label);
        button->set_action_name(spec.action);
        button->set_tooltip_text(spec.tooltip);
        m_header.pack_start(*button);
    }

    auto menu = Gio::Menu::create();
    auto files = Gio::Menu::create();
    files->append("Add Images…", "win.add-images");
    files->append("Add Folder…", "win.add-folder");
    menu->append_section(files);
    auto queue = Gio::Menu::create();
    queue->append("Select All", "win.select-all");
    queue->append("Remove Duplicates", "win.remove-duplicates");
    queue->append("Save As…", "win.save-as");
    menu->append_section(queue);
    auto view = Gio::Menu::create();
    view->append("Use System Title Bar", "win.system-title-bar");
    menu->append_section(view);
    auto help = Gio::Menu::create();
    help->append("Help", "win.help");
    help->append("About LGL Papercutter", "win.about");
    menu->append_section(help);
    auto quit = Gio::Menu::create();
    quit->append("Quit", "app.quit");
    menu->append_section(quit);
    m_menuButton = Gtk::make_managed<Gtk::MenuButton>();
    m_menuButton->set_icon_name("open-menu-symbolic");
    m_menuButton->set_menu_model(menu);
    m_menuButton->set_tooltip_text("Main menu");
    m_menuButton->set_primary(true);
    m_header.pack_end(*m_menuButton);

    auto *save = Gtk::make_managed<Gtk::Button>("Save As…");
    save->set_action_name("win.save-as");
    save->add_css_class("pc-primary");
    save->add_css_class("suggested-action");
    save->set_tooltip_text("Export the selected staged images as new files (Ctrl+Shift+S)");
    m_header.pack_end(*save);
}

// The header bar is either the window's title bar (client-side decorations, with the window
// buttons) or, when the compositor draws the title bar, the first row of the window.
// Switching between the two needs the window unrealized, so it is briefly hidden. gtkmm
// removes a hidden window from its application, which would quit; the application is held
// and the window re-added.
void Window::applyTitleBar(const bool system)
{
    const bool realized = get_realized();
    const Glib::RefPtr<Gtk::Application> application = get_application();
    if (realized) {
        application->hold();
        set_visible(false);
        Gtk::Widget::unrealize();
    }
    if (system) {
        unset_titlebar();
        m_header.set_show_title_buttons(false);
        m_content.prepend(m_header);
    } else {
        if (m_header.get_parent())
            m_content.remove(m_header);
        m_header.set_show_title_buttons(true);
        set_titlebar(m_header);
    }
    if (realized) {
        set_application(application);
        present();
        m_queue.focusList();
        application->release();
    }
}

void Window::buildBody()
{
    auto *inner = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    inner->add_css_class("body-paned");
    inner->set_start_child(m_canvas);
    inner->set_end_child(m_sidebar);
    inner->set_resize_start_child(true);
    inner->set_shrink_start_child(false);
    inner->set_resize_end_child(false);
    inner->set_shrink_end_child(false);

    auto *outer = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    outer->add_css_class("body-paned");
    outer->add_css_class("body");
    outer->set_start_child(m_queue);
    outer->set_end_child(*inner);
    outer->set_resize_start_child(false);
    outer->set_shrink_start_child(false);
    outer->set_shrink_end_child(false);
    outer->set_position(270);

    m_notice.set_wrap(true);
    m_notice.set_max_width_chars(80);
    m_noticeBox.add_css_class("pc-notice");
    m_noticeBox.append(m_notice);
    m_noticeRevealer.set_child(m_noticeBox);
    m_noticeRevealer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);
    m_noticeRevealer.set_halign(Gtk::Align::CENTER);
    m_noticeRevealer.set_valign(Gtk::Align::END);
    m_noticeRevealer.set_can_target(false);

    auto *overlay = Gtk::make_managed<Gtk::Overlay>();
    overlay->set_child(*outer);
    overlay->add_overlay(m_noticeRevealer);
    overlay->set_vexpand(true);
    m_content.append(*overlay);
    set_child(m_content);
}

void Window::buildActions(const Glib::RefPtr<Gtk::Application> &application)
{
    add_action("add-images", sigc::mem_fun(*this, &Window::addImages));
    add_action("add-folder", sigc::mem_fun(*this, &Window::addFolder));
    add_action("help", sigc::mem_fun(*this, &Window::showHelp));
    add_action("about", sigc::mem_fun(*this, &Window::showAbout));
    m_queueActions = {
        add_action("remove-selected", sigc::mem_fun(*this, &Window::removeSelectedImages)),
        add_action("trash-selected", sigc::mem_fun(*this, &Window::moveSelectedImagesToTrash)),
        add_action("save-as", sigc::mem_fun(*this, &Window::saveSelectedAs)),
        add_action("select-all", [this] { m_queue.selectAll(); }),
    };
    m_duplicatesAction =
        add_action("remove-duplicates", sigc::mem_fun(*this, &Window::removeDuplicates));
    auto titleBar = add_action_bool("system-title-bar", false);
    titleBar->signal_activate().connect([titleBar](const Glib::VariantBase &) {
        bool system = false;
        titleBar->get_state(system);
        titleBar->change_state(!system);
    });
    titleBar->signal_change_state().connect([this, titleBar](const Glib::VariantBase &value) {
        const bool system = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value).get();
        titleBar->set_state(value);
        m_settings.setSystemTitleBar(system);
        applyTitleBar(system);
    });
    titleBar->set_state(Glib::Variant<bool>::create(m_settings.systemTitleBar()));

    application->set_accel_for_action("win.add-images", "<Control>o");
    application->set_accel_for_action("win.add-folder", "<Control><Shift>o");
    application->set_accel_for_action("win.save-as", "<Control><Shift>s");
    application->set_accel_for_action("win.help", "F1");
    application->set_accel_for_action("win.system-title-bar", "<Control><Shift>m");
    application->set_accel_for_action("app.quit", "<Control>q");
}

void Window::updateActions()
{
    const bool hasItems = m_queue.size() > 0;
    for (const auto &action : m_queueActions)
        action->set_enabled(hasItems);
    m_duplicatesAction->set_enabled(m_queue.size() >= 2 && !m_checkingDuplicates);
}

void Window::notify(const std::string &text, const NoticeKind kind, const unsigned int timeoutMs)
{
    for (const char *name : {"positive", "neutral", "negative"})
        m_noticeBox.remove_css_class(name);
    switch (kind) {
    case NoticeKind::Positive:
        m_noticeBox.add_css_class("positive");
        break;
    case NoticeKind::Neutral:
        m_noticeBox.add_css_class("neutral");
        break;
    case NoticeKind::Negative:
        m_noticeBox.add_css_class("negative");
        break;
    case NoticeKind::Info:
        break;
    }
    m_notice.set_text(text);
    m_noticeRevealer.set_reveal_child(true);
    m_noticeTimer.disconnect();
    if (timeoutMs > 0)
        m_noticeTimer = Glib::signal_timeout().connect(
            [this] {
                m_noticeRevealer.set_reveal_child(false);
                return false;
            },
            timeoutMs);
}

// Alt pressed and released on its own opens or closes the main menu; closing it returns
// focus to the queue.
void Window::watchAltTap(Gtk::Widget &widget)
{
    auto keys = Gtk::EventControllerKey::create();
    keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) {
            m_altTapped = isAlt(keyval);
            return false;
        },
        false);
    keys->signal_key_released().connect([this](guint keyval, guint, Gdk::ModifierType) {
        if (m_altTapped && isAlt(keyval)) {
            if (m_menuButton->get_active()) {
                m_menuButton->popdown();
                // After the popover has handed focus back to the window.
                Glib::signal_idle().connect_once([this] { m_queue.focusList(); });
            } else
                m_menuButton->popup();
        }
        m_altTapped = false;
    });
    widget.add_controller(keys);
}

// --- resolution targets ----------------------------------------------------------

void Window::populateResolutionTargets(const bool selectDefault)
{
    const Size previousTarget = m_sidebar.targetSize();
    auto display = get_display();
    auto monitors = display->get_monitors();
    Glib::RefPtr<Gdk::Monitor> current;
    if (auto surface = get_surface())
        current = display->get_monitor_at_surface(surface);
    if (!current && monitors->get_n_items() > 0)
        current = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(0));

    std::vector<ResolutionChoice> choices;
    Size currentSize;
    const auto addMonitor = [&](const Glib::RefPtr<Gdk::Monitor> &monitor) {
        Gdk::Rectangle geometry;
        monitor->get_geometry(geometry);
        const double scale = monitor->get_scale();
        const Size pixels{static_cast<int>(std::lround(geometry.get_width() * scale)),
                          static_cast<int>(std::lround(geometry.get_height() * scale))};
        std::string label = monitor->get_connector();
        const std::string model = monitor->get_model();
        if (!model.empty())
            label += (label.empty() ? "" : " · ") + model;
        if (label.empty())
            label = "Display";
        if (monitor == current) {
            label += " (This display)";
            currentSize = pixels;
        }
        choices.push_back({label + " — " + sizeText(pixels), pixels, false});
    };
    if (current)
        addMonitor(current);
    for (guint i = 0; i < monitors->get_n_items(); ++i) {
        auto monitor = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i));
        if (monitor && monitor != current)
            addMonitor(monitor);
    }
    for (const Size &size : m_settings.resolutionPresets())
        choices.push_back({"Preset — " + sizeText(size), size, false});

    const bool wasUpdating = m_updatingControls;
    m_updatingControls = true;
    m_sidebar.setResolutionChoices(choices);
    const Size target = selectDefault && currentSize.isValid() ? currentSize : previousTarget;
    int targetIndex = m_sidebar.findResolution(target);
    if (targetIndex < 0 && target.isValid()) {
        m_sidebar.insertResolutionChoice(0, {"Current — " + sizeText(target), target, false});
        targetIndex = 0;
    }
    m_sidebar.selectResolution(targetIndex);
    if (target.isValid())
        m_sidebar.setTargetSize(target);
    m_updatingControls = wasUpdating;
    if (m_queue.size() > 0)
        syncResolutionControlsToSelection();
}

// --- queue -----------------------------------------------------------------------

void Window::addImages()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Add wallpapers");
    dialog->set_modal(true);
    if (!m_settings.lastSourceFolder().empty())
        dialog->set_initial_folder(Gio::File::create_for_path(m_settings.lastSourceFolder()));
    dialog->set_filters(imageFilters());
    dialog->open_multiple(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult> &result) {
        std::vector<Glib::RefPtr<Gio::File>> files;
        try {
            files = dialog->open_multiple_finish(result);
        } catch (const Glib::Error &) {
            return;
        }
        std::vector<std::string> paths;
        for (const auto &file : files) {
            if (file && !file->get_path().empty())
                paths.push_back(file->get_path());
        }
        enqueuePaths(paths);
    });
}

void Window::addFolder()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Add wallpaper folder");
    dialog->set_modal(true);
    if (!m_settings.lastSourceFolder().empty())
        dialog->set_initial_folder(Gio::File::create_for_path(m_settings.lastSourceFolder()));
    dialog->select_folder(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult> &result) {
        Glib::RefPtr<Gio::File> folder;
        try {
            folder = dialog->select_folder_finish(result);
        } catch (const Glib::Error &) {
            return;
        }
        if (folder && !folder->get_path().empty())
            enqueuePaths({folder->get_path()});
    });
}

void Window::enqueuePaths(const std::vector<std::string> &paths)
{
    if (paths.empty())
        return;

    std::set<std::string> queuedPaths;
    for (guint row = 0; row < m_queue.size(); ++row)
        queuedPaths.insert(absolutePath(m_queue.itemAt(static_cast<int>(row))->job.sourcePath));

    std::vector<std::string> candidateFiles;
    for (const std::string &input : paths) {
        const std::string path = absolutePath(input);
        if (Glib::file_test(path, Glib::FileTest::IS_DIR)) {
            std::vector<std::string> files;
            std::error_code error;
            for (const auto &entry : std::filesystem::directory_iterator(path, error)) {
                std::error_code statusError;
                const std::string name = entry.path().filename().string();
                if (name.empty() || name.front() == '.' || !entry.is_regular_file(statusError))
                    continue;
                if (isSupportedSuffix(fileSuffix(name))
                    && access(entry.path().c_str(), R_OK) == 0)
                    files.push_back(entry.path().string());
            }
            std::sort(files.begin(), files.end());
            candidateFiles.insert(candidateFiles.end(), files.begin(), files.end());
            m_settings.setLastSourceFolder(path);
        } else if (Glib::file_test(path, Glib::FileTest::IS_REGULAR)
                   && isSupportedSuffix(fileSuffix(path))) {
            candidateFiles.push_back(path);
            m_settings.setLastSourceFolder(Glib::path_get_dirname(path));
        }
    }

    std::vector<Glib::RefPtr<QueueItem>> added;
    for (const std::string &path : candidateFiles) {
        if (queuedPaths.count(path) != 0)
            continue;
        const auto size = readImageDimensions(path);
        if (!size)
            continue;
        ImageJob job;
        job.sourcePath = path;
        job.sourceSize = *size;
        job.composition.targetSize = m_sidebar.targetSize();
        added.push_back(QueueItem::create(job));
        queuedPaths.insert(path);
    }
    m_queue.appendItems(added);
    for (const auto &item : added) {
        requestThumbnail(item->job.sourcePath);
        m_metadataQueue.push_back(item->job.sourcePath);
    }
    pumpMetadata();
    notify("Added " + std::to_string(added.size()) + " image(s) to the queue.",
           added.empty() ? NoticeKind::Neutral : NoticeKind::Info, 5000);
}

void Window::requestThumbnail(const std::string &path)
{
    m_previewWorker.post([this, path] {
        LoadedImage image = loadImage(path, thumbnailMaxDimension);
        return [this, image] {
            const int row = rowForPath(image.path);
            if (row < 0 || !image.texture)
                return;
            auto item = m_queue.itemAt(row);
            item->thumbnail = image.texture;
            item->signalChanged().emit();
        };
    });
}

void Window::pumpMetadata()
{
    if (m_metadataRunning || m_metadataQueue.empty())
        return;
    const std::string path = m_metadataQueue.front();
    m_metadataQueue.pop_front();
    m_metadataRunning = true;
    ImageProcessor::readMetadataAsync(path, [this, path](const ImageMetadata &metadata) {
        m_metadataRunning = false;
        const int row = rowForPath(path);
        if (auto item = m_queue.itemAt(row); item && !item->job.metadataEdited) {
            item->job.metadata = metadata;
            item->metadataLoaded = true;
            if (row == m_queue.current()) {
                m_updatingControls = true;
                m_sidebar.description.set_text(metadata.description);
                m_sidebar.artist.set_text(metadata.artist);
                m_sidebar.copyright.set_text(metadata.copyright);
                m_sidebar.comment.set_text(metadata.comment);
                m_updatingControls = false;
            }
        }
        Glib::signal_idle().connect_once([this] { pumpMetadata(); });
    });
}

int Window::rowForPath(const std::string &path) const
{
    for (guint row = 0; row < m_queue.size(); ++row) {
        if (m_queue.itemAt(static_cast<int>(row))->job.sourcePath == path)
            return static_cast<int>(row);
    }
    return -1;
}

Glib::RefPtr<QueueItem> Window::currentItem() const
{
    return m_queue.itemAt(m_queue.current());
}

void Window::removeSelectedImages()
{
    m_queue.removeRows(m_queue.selectedRows());
}

void Window::removeRowsByPath(const std::vector<std::string> &paths)
{
    std::vector<int> rows;
    for (const std::string &path : paths) {
        const int row = rowForPath(path);
        if (row >= 0)
            rows.push_back(row);
    }
    m_queue.removeRows(rows);
}

void Window::moveSelectedImagesToTrash()
{
    const std::vector<int> rows = m_queue.selectedRows();
    if (rows.empty())
        return;
    std::vector<std::string> paths;
    for (const int row : rows) {
        if (auto item = m_queue.itemAt(row))
            paths.push_back(item->job.sourcePath);
    }
    askChoice(*this, "Move wallpapers to Trash",
              "Move " + std::to_string(paths.size())
                  + " selected wallpaper(s) to the desktop Trash?\n\n"
                    "This removes the source files, not just the queue entries.",
              {"Cancel", "Move to Trash"}, 0, 0, [this, paths](const int answer) {
                  if (answer != 1)
                      return;
                  std::vector<std::string> removed;
                  std::vector<std::string> failures;
                  for (const std::string &path : paths) {
                      bool trashed = false;
                      try {
                          trashed = Gio::File::create_for_path(path)->trash({});
                      } catch (const Glib::Error &) {
                      }
                      if (trashed)
                          removed.push_back(path);
                      else
                          failures.push_back(Glib::path_get_basename(path)
                                             + ": could not be moved to Trash");
                  }
                  removeRowsByPath(removed);
                  notify("Moved " + std::to_string(removed.size()) + " wallpaper(s) to Trash.",
                         failures.empty() ? NoticeKind::Info : NoticeKind::Negative, 5000);
                  if (!failures.empty()) {
                      std::string text;
                      for (const std::string &failure : failures)
                          text += (text.empty() ? "" : "\n") + failure;
                      showMessage(*this, "Some files were not moved", text);
                  }
              });
}

void Window::removeDuplicates()
{
    if (m_queue.size() < 2 || m_checkingDuplicates)
        return;
    notify("Checking for duplicate content…", NoticeKind::Info, 0);
    m_checkingDuplicates = true;
    updateActions();
    std::vector<std::string> paths;
    for (guint row = 0; row < m_queue.size(); ++row)
        paths.push_back(m_queue.itemAt(static_cast<int>(row))->job.sourcePath);
    m_backgroundWorker.post([this, paths] {
        std::set<std::string> hashes;
        std::vector<std::string> duplicates;
        for (const std::string &path : paths) {
            const std::string hash = fileSha256(path);
            if (hash.empty())
                continue;
            if (!hashes.insert(hash).second)
                duplicates.push_back(path);
        }
        return [this, duplicates] {
            m_checkingDuplicates = false;
            updateActions();
            removeRowsByPath(duplicates);
            notify("Removed " + std::to_string(duplicates.size()) + " duplicate queue item(s).",
                   NoticeKind::Info, 5000);
        };
    });
}

void Window::saveSelectedAs()
{
    std::vector<ImageJob> jobs;
    for (const int row : m_queue.selectedRows()) {
        if (auto item = m_queue.itemAt(row))
            jobs.push_back(item->job);
    }
    m_export.saveBatch(jobs);
}

void Window::showHelp()
{
    if (!m_help)
        m_help = std::make_unique<HelpWindow>(*this);
    m_help->setMonoFamily(m_theme->monoFamily());
    m_help->present();
}

void Window::showAbout()
{
    if (!m_about) {
        m_about = std::make_unique<Gtk::AboutDialog>();
        m_about->set_transient_for(*this);
        m_about->set_modal(true);
        m_about->set_hide_on_close(true);
        m_about->set_program_name("LGL Papercutter");
        m_about->set_version(PAPERCUTTER_VERSION);
        m_about->set_comments(
            "Compose wallpapers for your display resolution without altering the original "
            "images.\n\nImages are processed locally and exported as new files. "
            "Built with GTK 4 and ImageMagick.");
        m_about->set_website("https://github.com/linuxgamerlife/lgl-papercutter");
        m_about->set_website_label("Project page and issue tracker");
        m_about->set_authors({"LinuxGamerLife https://www.youtube.com/@linuxgamerlife"});
        m_about->add_credit_section("Support",
                                    {"Support development on Ko-fi https://ko-fi.com/G2G3V70LW"});
        m_about->set_license_type(Gtk::License::MIT_X11);
        m_about->set_logo(appIcon());
    }
    m_about->present();
}

// --- current job -----------------------------------------------------------------

void Window::clearCurrentJob()
{
    ++m_previewGeneration;
    m_canvas.clearImage("Add an image to begin composing");
    m_sidebar.setSourcePath({});
    m_updatingControls = true;
    m_sidebar.setOutputEnabled(false);
    m_sidebar.setAttributesEnabled(false);
    for (Gtk::Entry *field : {&m_sidebar.description, &m_sidebar.artist, &m_sidebar.copyright,
                              &m_sidebar.comment})
        field->set_text("");
    m_updatingControls = false;
    updateQualityLabel();
}

void Window::loadCurrentJob()
{
    auto item = currentItem();
    if (!item) {
        clearCurrentJob();
        return;
    }

    const ImageJob &job = item->job;
    m_sidebar.setSourcePath(job.sourcePath);
    m_canvas.setComposition(job.composition);

    const auto cached = std::find_if(m_previewCache.begin(), m_previewCache.end(),
                                     [&job](const LoadedImage &image) {
                                         return image.path == job.sourcePath;
                                     });
    const unsigned long long generation = ++m_previewGeneration;
    if (cached != m_previewCache.end()) {
        showPreview(*cached);
    } else {
        m_canvas.clearImage("Loading…");
        m_previewWorker.post(
            [this, generation, path = job.sourcePath] {
                LoadedImage image = loadImage(path, previewMaxDimension);
                return [this, generation, image] {
                    if (image.texture) {
                        m_previewCache.push_front(image);
                        if (m_previewCache.size() > previewCacheSize)
                            m_previewCache.pop_back();
                    }
                    if (generation == m_previewGeneration)
                        showPreview(image);
                };
            },
            true);
    }

    m_updatingControls = true;
    m_sidebar.setTargetSize(job.composition.targetSize);
    m_sidebar.setZoomPercent(static_cast<int>(std::lround(job.composition.zoom * 100.0)));
    m_sidebar.setOutputQuality(job.outputQuality);
    const std::string suffix = fileSuffix(job.sourcePath);
    const bool qualityApplies = suffix != "png";
    m_sidebar.setOutputFormat(qualityApplies
                                  ? "OUTPUT QUALITY (" + uppercase(outputFormatName(suffix)) + ")"
                                  : std::string("PNG COMPRESSION"),
                              qualityApplies);
    m_sidebar.description.set_text(job.metadata.description);
    m_sidebar.artist.set_text(job.metadata.artist);
    m_sidebar.copyright.set_text(job.metadata.copyright);
    m_sidebar.comment.set_text(job.metadata.comment);
    m_sidebar.setAttributesEnabled(!ImageProcessor::metadataExecutable().empty());
    m_updatingControls = false;
    syncResolutionControlsToSelection();
    updateQualityLabel();
}

void Window::showPreview(const LoadedImage &image)
{
    auto item = currentItem();
    if (!item || item->job.sourcePath != image.path)
        return;
    if (!image.texture) {
        m_canvas.clearImage("Unable to load image");
        showMessage(*this, "Unable to load image", image.error);
        return;
    }
    if (image.orientedSize)
        item->job.sourceSize = *image.orientedSize;
    m_canvas.setImage(image.texture, item->job.sourceSize);
    m_canvas.setComposition(item->job.composition);
    updateQualityLabel();
}

void Window::applyTargetResolutionToSelection()
{
    if (m_updatingControls)
        return;
    const std::vector<int> rows = m_queue.selectedRows();
    if (rows.empty())
        return;

    const Size targetSize = m_sidebar.targetSize();
    for (const int row : rows) {
        if (auto item = m_queue.itemAt(row)) {
            item->job.composition.targetSize = targetSize;
            item->signalChanged().emit();
        }
    }
    if (auto item = currentItem())
        m_canvas.setComposition(item->job.composition);
    syncResolutionControlsToSelection();
    updateQualityLabel();
}

void Window::updateCurrentZoomFromControl()
{
    if (m_updatingControls)
        return;
    auto item = currentItem();
    if (!item)
        return;
    item->job.composition.zoom = m_sidebar.zoomPercent() / 100.0;
    m_canvas.setComposition(item->job.composition);
    updateQualityLabel();
}

void Window::updateCurrentOutputQuality()
{
    if (m_updatingControls)
        return;
    auto item = currentItem();
    if (!item)
        return;
    item->job.outputQuality = m_sidebar.outputQuality();
    updateQualityLabel();
}

void Window::updateCurrentMetadataFromControls()
{
    if (m_updatingControls)
        return;
    auto item = currentItem();
    if (!item)
        return;
    ImageJob &job = item->job;
    job.metadata.description = m_sidebar.description.get_text();
    job.metadata.artist = m_sidebar.artist.get_text();
    job.metadata.copyright = m_sidebar.copyright.get_text();
    job.metadata.comment = m_sidebar.comment.get_text();
    job.metadataEdited = true;
}

void Window::syncResolutionControlsToSelection()
{
    if (m_updatingControls)
        return;
    const std::vector<int> rows = m_queue.selectedRows();
    auto current = currentItem();
    if (rows.empty() || !current)
        return;

    const Size currentSize = current->job.composition.targetSize;
    const Size firstSize = m_queue.itemAt(rows.front())->job.composition.targetSize;
    bool mixed = false;
    for (const int row : rows) {
        auto item = m_queue.itemAt(row);
        if (item && item->job.composition.targetSize != firstSize) {
            mixed = true;
            break;
        }
    }

    m_updatingControls = true;
    const auto &choices = m_sidebar.resolutionChoices();
    for (int index = static_cast<int>(choices.size()) - 1; index >= 0; --index) {
        if (choices[static_cast<std::size_t>(index)].transient)
            m_sidebar.removeResolutionChoice(index);
    }
    if (mixed) {
        m_sidebar.insertResolutionChoice(0, {"Mixed resolutions", {}, true});
        m_sidebar.selectResolution(0);
    } else {
        int index = m_sidebar.findResolution(firstSize);
        if (index < 0) {
            m_sidebar.insertResolutionChoice(0, {"Staged — " + sizeText(firstSize), firstSize, true});
            index = 0;
        }
        m_sidebar.selectResolution(index);
    }
    m_sidebar.setTargetSize(currentSize);
    m_updatingControls = false;
}

// --- read-outs -------------------------------------------------------------------

void Window::updateQualityLabel()
{
    auto item = currentItem();
    if (!item) {
        m_sidebar.setScaleInfo(ScaleState::None, 0.0);
        m_sidebar.setFileSizes({}, "Estimated output", {});
        ++m_estimateGeneration;
        m_estimateTimer.disconnect();
        return;
    }
    const ImageJob &job = item->job;
    const double scale = job.composition.effectiveScaleFor(job.sourceSize);
    const ScaleState state = scale <= 1.0 ? ScaleState::Sharp
        : scale <= 1.5                    ? ScaleState::Enlarged
                                          : ScaleState::Soft;
    m_sidebar.setScaleInfo(state, scale);
    scheduleOutputSizeEstimate();
}

void Window::scheduleOutputSizeEstimate()
{
    auto item = currentItem();
    if (!item)
        return;
    ++m_estimateGeneration;
    const ImageJob &job = item->job;
    m_sidebar.setFileSizes(formatFileSize(fileSize(job.sourcePath)),
                           "Estimated " + outputFormatName(fileSuffix(job.sourcePath)),
                           "calculating…");
    m_estimateTimer.disconnect();
    m_estimateTimer = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &Window::runOutputSizeEstimate), estimateDelayMs);
}

bool Window::runOutputSizeEstimate()
{
    if (m_estimateRunning)
        return true; // Try again after the running estimate finishes.
    auto item = currentItem();
    if (!item)
        return false;
    const ImageJob job = item->job;
    const std::string suffix = fileSuffix(job.sourcePath);
    const unsigned long long generation = m_estimateGeneration;
    m_estimateRunning = true;
    ImageProcessor::estimateOutputSizeAsync(
        job, suffix, [this, generation, sourcePath = job.sourcePath, suffix](long long bytes) {
            m_estimateRunning = false;
            auto current = currentItem();
            if (generation != m_estimateGeneration || !current
                || current->job.sourcePath != sourcePath)
                return;
            m_sidebar.setFileSizes(formatFileSize(fileSize(sourcePath)),
                                   "Estimated " + outputFormatName(suffix),
                                   formatFileSize(bytes));
        });
    return false;
}

} // namespace papercutter
