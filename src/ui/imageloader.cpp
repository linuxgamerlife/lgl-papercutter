#include "ui/imageloader.h"

#include "core/imageinfo.h"
#include "core/processor.h"

#include <gdkmm/memorytexture.h>
#include <gdkmm/pixbuf.h>
#include <giomm/subprocess.h>

#include <algorithm>

namespace papercutter {

Worker::Worker()
{
    m_dispatcher.connect(sigc::mem_fun(*this, &Worker::deliver));
    m_thread = std::thread(&Worker::run, this);
}

Worker::~Worker()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
        m_tasks.clear();
    }
    m_wake.notify_all();
    m_thread.join();
}

void Worker::post(Task task, const bool urgent)
{
    {
        std::lock_guard lock(m_mutex);
        if (urgent)
            m_tasks.push_front(std::move(task));
        else
            m_tasks.push_back(std::move(task));
    }
    m_wake.notify_one();
}

void Worker::run()
{
    for (;;) {
        Task task;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
            if (m_stopping)
                return;
            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }
        Completion completion = task();
        {
            std::lock_guard lock(m_mutex);
            if (m_stopping)
                return;
            m_completions.push_back(std::move(completion));
        }
        m_dispatcher.emit();
    }
}

void Worker::deliver()
{
    std::deque<Completion> completions;
    {
        std::lock_guard lock(m_mutex);
        completions.swap(m_completions);
    }
    for (Completion &completion : completions) {
        if (completion)
            completion();
    }
}

namespace {

Glib::RefPtr<Gdk::Texture> textureFromPixbuf(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf)
{
    const int width = pixbuf->get_width();
    const int height = pixbuf->get_height();
    const int stride = pixbuf->get_rowstride();
    const int channels = pixbuf->get_n_channels();
    if (pixbuf->get_bits_per_sample() != 8 || (channels != 3 && channels != 4))
        return {};
    const gsize length = static_cast<gsize>(stride) * static_cast<gsize>(height - 1)
        + static_cast<gsize>(width) * static_cast<gsize>(channels);
    auto bytes = Glib::Bytes::create(pixbuf->get_pixels(), length);
    return Gdk::MemoryTexture::create(width, height,
                                      channels == 4 ? Gdk::MemoryTexture::Format::R8G8B8A8
                                                    : Gdk::MemoryTexture::Format::R8G8B8,
                                      bytes, static_cast<gsize>(stride));
}

Glib::RefPtr<Gdk::Texture> loadWithImageMagick(const std::string &path, const int maxDimension)
{
    const std::string executable = ImageProcessor::backendExecutable();
    if (executable.empty())
        return {};
    const std::string bound = std::to_string(maxDimension) + 'x' + std::to_string(maxDimension)
        + '>';
    try {
        auto process = Gio::Subprocess::create(
            {executable, path + "[0]", "-auto-orient", "-resize", bound, "png:-"},
            Gio::Subprocess::Flags::STDOUT_PIPE | Gio::Subprocess::Flags::STDERR_SILENCE);
        auto [output, errors] = process->communicate({});
        if (!process->get_successful() || !output)
            return {};
        return Gdk::Texture::create_from_bytes(output);
    } catch (const Glib::Error &) {
        return {};
    }
}

} // namespace

LoadedImage loadImage(const std::string &path, const int maxDimension)
{
    LoadedImage image;
    image.path = path;
    image.orientedSize = readImageDimensions(path);
    int width = 0;
    int height = 0;
    try {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf;
        const bool large = image.orientedSize
            && std::max(image.orientedSize->width, image.orientedSize->height) > maxDimension;
        pixbuf = large ? Gdk::Pixbuf::create_from_file(path, maxDimension, maxDimension, true)
                       : Gdk::Pixbuf::create_from_file(path);
        if (auto oriented = pixbuf->apply_embedded_orientation())
            pixbuf = oriented;
        width = pixbuf->get_width();
        height = pixbuf->get_height();
        image.texture = textureFromPixbuf(pixbuf);
    } catch (const Glib::Error &error) {
        image.error = error.what();
    }
    if (!image.texture) {
        image.texture = loadWithImageMagick(path, maxDimension);
        if (image.texture) {
            width = image.texture->get_width();
            height = image.texture->get_height();
            image.error.clear();
        } else if (image.error.empty()) {
            image.error = "The image could not be decoded.";
        }
    }
    // The header size is authoritative; only correct its orientation when the decoder
    // rotated an image the header parser could not (for example PNG eXIf).
    if (image.texture && image.orientedSize && width != height
        && image.orientedSize->width != image.orientedSize->height
        && (width > height) != (image.orientedSize->width > image.orientedSize->height))
        std::swap(image.orientedSize->width, image.orientedSize->height);
    if (image.texture && !image.orientedSize)
        image.orientedSize = Size{width, height};
    return image;
}

} // namespace papercutter
