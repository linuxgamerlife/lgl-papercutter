#pragma once

#include "core/settings.h"
#include "ui/canvas.h"
#include "ui/exportflow.h"
#include "ui/imageloader.h"
#include "ui/queuelist.h"
#include "ui/sidebar.h"

#include <giomm/simpleaction.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/revealer.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace papercutter {

class HelpWindow;
class ThemeWatcher;

// The main window. Port of src/ui/mainwindow.cpp: per-image staged composition, quality and
// attributes; queue management; Save As; Trash; duplicate removal.
class Window final : public Gtk::ApplicationWindow {
public:
    explicit Window(const Glib::RefPtr<Gtk::Application> &application);
    ~Window() override;

    // Adds image files and the supported images directly inside folders (not recursive).
    void enqueuePaths(const std::vector<std::string> &paths);

protected:
    void on_realize() override;

private:
    void buildHeader();
    void applyTitleBar(bool system);
    void buildBody();
    void buildActions(const Glib::RefPtr<Gtk::Application> &application);
    void updateActions();

    void populateResolutionTargets(bool selectDefault);
    void addImages();
    void addFolder();
    void removeSelectedImages();
    void removeRowsByPath(const std::vector<std::string> &paths);
    void moveSelectedImagesToTrash();
    void removeDuplicates();
    void saveSelectedAs();
    void showHelp();
    void showAbout();

    void loadCurrentJob();
    void clearCurrentJob();
    void showPreview(const LoadedImage &image);
    void applyTargetResolutionToSelection();
    void updateCurrentZoomFromControl();
    void updateCurrentOutputQuality();
    void updateCurrentMetadataFromControls();
    void syncResolutionControlsToSelection();
    void updateQualityLabel();
    void scheduleOutputSizeEstimate();
    bool runOutputSizeEstimate();

    void requestThumbnail(const std::string &path);
    void pumpMetadata();
    int rowForPath(const std::string &path) const;
    Glib::RefPtr<QueueItem> currentItem() const;

    void notify(const std::string &text, NoticeKind kind, unsigned int timeoutMs);
    void watchAltTap(Gtk::Widget &widget);

    Settings m_settings;
    QueueList m_queue;
    Canvas m_canvas;
    Sidebar m_sidebar;
    // A member, not managed, so it survives moving between the title bar and m_content.
    Gtk::HeaderBar m_header;
    Gtk::Box m_content;
    Gtk::Revealer m_noticeRevealer;
    Gtk::Box m_noticeBox;
    Gtk::Label m_notice;
    ExportFlow m_export;
    std::unique_ptr<ThemeWatcher> m_theme;
    std::unique_ptr<HelpWindow> m_help;
    std::unique_ptr<Gtk::AboutDialog> m_about;
    std::vector<Glib::RefPtr<Gio::SimpleAction>> m_queueActions;
    Glib::RefPtr<Gio::SimpleAction> m_duplicatesAction;
    Gtk::MenuButton *m_menuButton{nullptr};
    bool m_altTapped{false};

    sigc::connection m_estimateTimer;
    sigc::connection m_noticeTimer;
    unsigned long long m_estimateGeneration{0};
    bool m_estimateRunning{false};
    bool m_updatingControls{false};
    bool m_defaultMonitorApplied{false};
    unsigned long long m_previewGeneration{0};
    std::deque<LoadedImage> m_previewCache;
    std::deque<std::string> m_metadataQueue;
    bool m_metadataRunning{false};
    bool m_checkingDuplicates{false};

    // Declared last so their threads stop before anything a completion might touch.
    Worker m_previewWorker;
    Worker m_backgroundWorker;
};

} // namespace papercutter
