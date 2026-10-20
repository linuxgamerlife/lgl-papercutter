#pragma once

#include "core/composition.h"

#include <gdkmm/texture.h>
#include <glibmm/dispatcher.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace papercutter {

// A single background thread. post() runs work off the main thread; the completion it
// returns is then run on the main thread. Pending completions are dropped on destruction.
class Worker final {
public:
    using Completion = std::function<void()>;
    using Task = std::function<Completion()>;

    Worker();
    ~Worker();
    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;

    void post(Task task, bool urgent = false);

private:
    void run();
    void deliver();

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<Task> m_tasks;
    std::deque<Completion> m_completions;
    bool m_stopping{false};
    Glib::Dispatcher m_dispatcher;
    std::thread m_thread;
};

struct LoadedImage {
    std::string path;
    Glib::RefPtr<Gdk::Texture> texture;
    // Full image size as displayed (EXIF orientation applied).
    std::optional<Size> orientedSize;
    std::string error;
};

// Decodes path scaled to fit maxDimension × maxDimension with its orientation applied.
// Uses gdk-pixbuf and falls back to ImageMagick for formats without a loader. Thread-safe.
LoadedImage loadImage(const std::string &path, int maxDimension);

} // namespace papercutter
