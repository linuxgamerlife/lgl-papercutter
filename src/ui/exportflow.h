#pragma once

#include "core/imagejob.h"
#include "core/settings.h"

#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace papercutter {

enum class NoticeKind { Info, Positive, Neutral, Negative };

// Save As for one image (file picker) or several (folder picker), ported from
// MainWindow::saveAsCurrent / saveSelectedAs: optional sequential numbering that fills
// gaps, replace / skip / cancel on collisions, a source → destination preview, and then
// asynchronous rendering. Never replaces a source image.
class ExportFlow final {
public:
    using Notify = std::function<void(const std::string &text, NoticeKind kind,
                                      unsigned int timeoutMs)>;

    ExportFlow(Gtk::Window &parent, Settings &settings, Notify notify);

    void saveSingle(const ImageJob &job);
    void saveBatch(const std::vector<ImageJob> &jobs);

private:
    struct Batch;

    void chooseNumbering(const std::string &folder, std::function<void(bool, bool)> done);
    void continueSingle(const ImageJob &job, std::string destination);
    void planBatch(const std::shared_ptr<Batch> &batch, std::size_t index);
    void confirmBatch(const std::shared_ptr<Batch> &batch);
    void runBatch(const std::shared_ptr<Batch> &batch, std::size_t index);

    Gtk::Window &m_parent;
    Settings &m_settings;
    Notify m_notify;
};

} // namespace papercutter
