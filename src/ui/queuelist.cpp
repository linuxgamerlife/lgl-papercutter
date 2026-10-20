#include "ui/queuelist.h"

#include <gdk/gdkkeysyms.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/picture.h>
#include <gtkmm/shortcutcontroller.h>
#include <gtkmm/signallistitemfactory.h>

#include <algorithm>

namespace papercutter {

namespace {

std::string resolutionText(const Size &size)
{
    return std::to_string(size.width) + " × " + std::to_string(size.height);
}

// The widget for one row; remembers its list item while bound.
class QueueRow final : public Gtk::Box {
public:
    QueueRow()
        : Gtk::Box(Gtk::Orientation::HORIZONTAL, 10)
        , m_text(Gtk::Orientation::VERTICAL, 2)
    {
        m_thumbnail.add_css_class("queue-thumb");
        m_thumbnail.set_content_fit(Gtk::ContentFit::COVER);
        m_thumbnail.set_size_request(56, 36);
        m_thumbnail.set_can_shrink(true);
        m_thumbnail.set_overflow(Gtk::Overflow::HIDDEN);
        m_thumbnail.set_valign(Gtk::Align::CENTER);
        m_name.set_xalign(0.0f);
        m_name.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
        m_resolution.set_xalign(0.0f);
        m_resolution.add_css_class("mono");
        m_resolution.add_css_class("small");
        m_resolution.add_css_class("muted");
        m_text.set_valign(Gtk::Align::CENTER);
        m_text.set_hexpand(true);
        m_text.append(m_name);
        m_text.append(m_resolution);
        append(m_thumbnail);
        append(m_text);
    }

    void bind(Gtk::ListItem *listItem, const Glib::RefPtr<QueueItem> &item)
    {
        m_listItem = listItem;
        m_item = item;
        m_connection = item->signalChanged().connect(sigc::mem_fun(*this, &QueueRow::refresh));
        refresh();
    }

    void unbind()
    {
        m_connection.disconnect();
        m_listItem = nullptr;
        m_item.reset();
    }

    int position() const
    {
        return m_listItem ? static_cast<int>(m_listItem->get_position()) : -1;
    }

private:
    void refresh()
    {
        if (!m_item)
            return;
        m_thumbnail.set_paintable(m_item->thumbnail);
        m_name.set_text(m_item->job.displayName());
        set_tooltip_text(m_item->job.sourcePath);
        m_resolution.set_text(resolutionText(m_item->job.composition.targetSize));
    }

    Gtk::Picture m_thumbnail;
    Gtk::Box m_text;
    Gtk::Label m_name;
    Gtk::Label m_resolution;
    Gtk::ListItem *m_listItem{nullptr};
    Glib::RefPtr<QueueItem> m_item;
    sigc::connection m_connection;
};

} // namespace

Glib::RefPtr<QueueItem> QueueItem::create(ImageJob job)
{
    return Glib::make_refptr_for_instance<QueueItem>(new QueueItem(std::move(job)));
}

QueueItem::QueueItem(ImageJob job)
    : Glib::ObjectBase(typeid(QueueItem))
    , job(std::move(job))
{
}

QueueList::QueueList()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , m_store(Gio::ListStore<QueueItem>::create())
    , m_selection(Gtk::MultiSelection::create(m_store))
{
    add_css_class("pc-card");
    add_css_class("flush");
    // Clip the list to the card's rounded corners.
    set_overflow(Gtk::Overflow::HIDDEN);
    set_size_request(240, -1);

    auto head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    head->add_css_class("card-head");
    auto title = Gtk::make_managed<Gtk::Label>("QUEUE");
    title->add_css_class("section-label");
    title->set_xalign(0.0f);
    title->set_hexpand(true);
    m_count.add_css_class("mono");
    m_count.add_css_class("small");
    m_count.add_css_class("muted");
    head->append(*title);
    head->append(m_count);
    append(*head);

    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(sigc::mem_fun(*this, &QueueList::setupRow));
    factory->signal_bind().connect(sigc::mem_fun(*this, &QueueList::bindRow));
    factory->signal_unbind().connect(sigc::mem_fun(*this, &QueueList::unbindRow));
    m_view.set_factory(factory);
    m_view.set_model(m_selection);
    m_view.add_css_class("queue-list");
    gtk_accessible_update_property(GTK_ACCESSIBLE(m_view.gobj()), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   "Processing queue", -1);

    // Remember the row under the pointer so a click makes that row current, and open the
    // context menu on secondary clicks.
    auto click = Gtk::GestureClick::create();
    click->set_button(0);
    click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    click->signal_pressed().connect([this, click](int, double x, double y) {
        m_lastPressed = positionAt(x, y);
        if (click->get_current_button() == GDK_BUTTON_SECONDARY) {
            click->set_state(Gtk::EventSequenceState::CLAIMED);
            showContextMenu(x, y);
        }
    });
    m_view.add_controller(click);

    auto shortcuts = Gtk::ShortcutController::create();
    shortcuts->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_Delete),
        Gtk::NamedAction::create("win.remove-selected")));
    shortcuts->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_Menu), Gtk::CallbackAction::create(
            [this](Gtk::Widget &, const Glib::VariantBase &) {
                showContextMenu(m_view.get_width() / 2.0, 24.0);
                return true;
            })));
    m_view.add_controller(shortcuts);

    m_scroller.set_child(m_view);
    m_scroller.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    m_scroller.set_vexpand(true);

    auto empty = Gtk::make_managed<Gtk::Label>(
        "Drop images or folders here,\nor use Add Images.");
    empty->add_css_class("muted");
    empty->add_css_class("queue-empty");
    empty->set_justify(Gtk::Justification::CENTER);
    empty->set_wrap(true);
    m_stack.add(*empty, "empty");
    m_stack.add(m_scroller, "list");
    m_stack.set_vexpand(true);
    append(m_stack);

    auto menu = Gio::Menu::create();
    menu->append("Save As…", "win.save-as");
    auto destructive = Gio::Menu::create();
    destructive->append("Move to Trash…", "win.trash-selected");
    destructive->append("Remove from Queue", "win.remove-selected");
    menu->append_section(destructive);
    m_menu.set_menu_model(menu);
    m_menu.set_has_arrow(false);
    m_menu.set_halign(Gtk::Align::START);
    m_menu.set_parent(m_view);

    m_selection->signal_selection_changed().connect(
        sigc::mem_fun(*this, &QueueList::onSelectionChanged));
    m_store->signal_items_changed().connect(
        [this](guint, guint, guint) { updateHeader(); });
    updateHeader();
}

QueueList::~QueueList()
{
    m_menu.unparent();
}

Glib::RefPtr<QueueItem> QueueList::itemAt(const int position) const
{
    if (position < 0 || position >= static_cast<int>(size()))
        return {};
    return m_store->get_item(static_cast<guint>(position));
}

std::vector<int> QueueList::selectedRows() const
{
    std::vector<int> rows;
    for (guint position = 0; position < size(); ++position) {
        if (m_selection->is_selected(position))
            rows.push_back(static_cast<int>(position));
    }
    if (rows.empty() && m_current >= 0)
        rows.push_back(m_current);
    return rows;
}

void QueueList::appendItems(const std::vector<Glib::RefPtr<QueueItem>> &items)
{
    if (items.empty())
        return;
    m_store->splice(size(), 0, items);
    if (m_current < 0)
        selectOnly(0);
}

void QueueList::removeRows(const std::vector<int> &rows)
{
    if (rows.empty())
        return;
    const int nextRow = *std::min_element(rows.begin(), rows.end());
    std::vector<int> descending = rows;
    std::sort(descending.rbegin(), descending.rend());
    descending.erase(std::unique(descending.begin(), descending.end()), descending.end());
    m_changingSelection = true;
    for (const int row : descending) {
        if (row >= 0 && row < static_cast<int>(size()))
            m_store->remove(static_cast<guint>(row));
    }
    m_changingSelection = false;
    m_current = -1;
    if (size() > 0) {
        selectOnly(std::min(nextRow, static_cast<int>(size()) - 1));
    } else {
        m_currentChanged.emit();
        m_selectionChanged.emit();
    }
}

void QueueList::selectOnly(const int position)
{
    if (position < 0 || position >= static_cast<int>(size()))
        return;
    m_changingSelection = true;
    m_selection->select_item(static_cast<guint>(position), true);
    m_changingSelection = false;
    m_view.scroll_to(static_cast<guint>(position), Gtk::ListScrollFlags::NONE);
    setCurrent(position);
    m_selectionChanged.emit();
}

void QueueList::selectRows(const std::vector<int> &rows)
{
    m_changingSelection = true;
    m_selection->unselect_all();
    for (const int row : rows) {
        if (row >= 0 && row < static_cast<int>(size()))
            m_selection->select_item(static_cast<guint>(row), false);
    }
    m_changingSelection = false;
    m_selectionChanged.emit();
}

void QueueList::selectAll()
{
    m_selection->select_all();
}

void QueueList::focusList()
{
    if (size() > 0)
        m_view.grab_focus();
}

void QueueList::setupRow(const Glib::RefPtr<Gtk::ListItem> &listItem)
{
    listItem->set_child(*Gtk::make_managed<QueueRow>());
}

void QueueList::bindRow(const Glib::RefPtr<Gtk::ListItem> &listItem)
{
    auto *row = dynamic_cast<QueueRow *>(listItem->get_child());
    auto item = std::dynamic_pointer_cast<QueueItem>(listItem->get_item());
    if (row && item)
        row->bind(listItem.get(), item);
}

void QueueList::unbindRow(const Glib::RefPtr<Gtk::ListItem> &listItem)
{
    if (auto *row = dynamic_cast<QueueRow *>(listItem->get_child()))
        row->unbind();
}

int QueueList::positionAt(const double x, const double y)
{
    for (Gtk::Widget *widget = m_view.pick(x, y); widget && widget != &m_view;
         widget = widget->get_parent()) {
        if (auto *row = dynamic_cast<QueueRow *>(widget))
            return row->position();
    }
    return -1;
}

void QueueList::onSelectionChanged(const guint position, const guint count)
{
    if (m_changingSelection)
        return;
    int candidate = -1;
    if (m_lastPressed >= 0 && m_selection->is_selected(static_cast<guint>(m_lastPressed))) {
        candidate = m_lastPressed;
    } else {
        int selectedInRange = 0;
        for (guint row = position; row < position + count && row < size(); ++row) {
            if (m_selection->is_selected(row)) {
                ++selectedInRange;
                candidate = static_cast<int>(row);
            }
        }
        if (selectedInRange != 1)
            candidate = -1;
    }
    m_lastPressed = -1;
    if (candidate < 0 && m_current >= 0
        && m_selection->is_selected(static_cast<guint>(m_current)))
        candidate = m_current;
    if (candidate < 0) {
        const auto rows = selectedRows();
        candidate = rows.empty() ? m_current : rows.front();
    }
    setCurrent(candidate);
    m_selectionChanged.emit();
}

void QueueList::setCurrent(const int position)
{
    if (position == m_current)
        return;
    m_current = position;
    m_currentChanged.emit();
}

void QueueList::showContextMenu(const double x, const double y)
{
    const int clicked = m_lastPressed >= 0 ? m_lastPressed : m_current;
    if (clicked < 0)
        return;
    if (!m_selection->is_selected(static_cast<guint>(clicked)))
        selectOnly(clicked);
    m_lastPressed = -1;
    m_menu.set_pointing_to(Gdk::Rectangle(static_cast<int>(x), static_cast<int>(y), 1, 1));
    m_menu.popup();
}

void QueueList::updateHeader()
{
    const guint count = size();
    m_count.set_text(count == 0 ? "" : std::to_string(count));
    m_stack.set_visible_child(count == 0 ? "empty" : "list");
}

} // namespace papercutter
