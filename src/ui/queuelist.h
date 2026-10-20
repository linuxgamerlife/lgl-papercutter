#pragma once

#include "core/imagejob.h"

#include <gdkmm/texture.h>
#include <giomm/liststore.h>
#include <giomm/menu.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/listitem.h>
#include <gtkmm/listview.h>
#include <gtkmm/multiselection.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stack.h>

#include <vector>

namespace papercutter {

// One queued image: its staged job plus the queue thumbnail.
class QueueItem final : public Glib::Object {
public:
    static Glib::RefPtr<QueueItem> create(ImageJob job);

    ImageJob job;
    Glib::RefPtr<Gdk::Texture> thumbnail;
    bool metadataLoaded{false};

    // Emitted when the row should be redrawn (thumbnail or staged resolution changed).
    sigc::signal<void()> &signalChanged() { return m_changed; }

private:
    explicit QueueItem(ImageJob job);

    sigc::signal<void()> m_changed;
};

// The queue card: a multi-selection list (Ctrl/Shift click, Ctrl+A, Delete) with
// thumbnails, file names and staged resolutions, plus a right-click menu. The "current"
// item follows Qt's currentRow: the item last clicked or keyboard-selected.
class QueueList final : public Gtk::Box {
public:
    QueueList();
    ~QueueList() override;

    Glib::RefPtr<Gio::ListStore<QueueItem>> store() const { return m_store; }
    guint size() const { return m_store->get_n_items(); }
    Glib::RefPtr<QueueItem> itemAt(int position) const;

    // The current row, or -1.
    int current() const { return m_current; }
    // Selected rows in ascending order; the current row when nothing is selected.
    std::vector<int> selectedRows() const;

    void appendItems(const std::vector<Glib::RefPtr<QueueItem>> &items);
    // Removes rows and makes min(lowest removed row, last row) current.
    void removeRows(const std::vector<int> &rows);
    void selectOnly(int position);
    void selectRows(const std::vector<int> &rows);
    void selectAll();
    // Moves keyboard focus to the list; does nothing while the queue is empty.
    void focusList();

    sigc::signal<void()> &signalCurrentChanged() { return m_currentChanged; }
    sigc::signal<void()> &signalSelectionChanged() { return m_selectionChanged; }

private:
    void setupRow(const Glib::RefPtr<Gtk::ListItem> &listItem);
    void bindRow(const Glib::RefPtr<Gtk::ListItem> &listItem);
    void unbindRow(const Glib::RefPtr<Gtk::ListItem> &listItem);
    int positionAt(double x, double y);
    void onSelectionChanged(guint position, guint count);
    void setCurrent(int position);
    void showContextMenu(double x, double y);
    void updateHeader();

    Glib::RefPtr<Gio::ListStore<QueueItem>> m_store;
    Glib::RefPtr<Gtk::MultiSelection> m_selection;
    Gtk::Label m_count;
    Gtk::Stack m_stack;
    Gtk::ScrolledWindow m_scroller;
    Gtk::ListView m_view;
    Gtk::PopoverMenu m_menu;
    int m_current{-1};
    int m_lastPressed{-1};
    bool m_changingSelection{false};
    sigc::signal<void()> m_currentChanged;
    sigc::signal<void()> m_selectionChanged;
};

} // namespace papercutter
