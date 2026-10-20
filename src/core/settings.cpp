#include "core/settings.h"

#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cstdio>

namespace papercutter {

namespace {

bool parseSize(const std::string &text, Size &size)
{
    int width = 0;
    int height = 0;
    char trailing = 0;
    if (std::sscanf(text.c_str(), "%dx%d%c", &width, &height, &trailing) != 2)
        return false;
    size = {width, height};
    return size.isValid();
}

} // namespace

Settings::Settings()
    : Settings(Glib::build_filename(Glib::get_user_config_dir(), "lgl-papercutter",
                                    "settings.ini"))
{
}

Settings::Settings(std::string path)
    : m_path(std::move(path))
    , m_file(Glib::KeyFile::create())
{
    try {
        m_file->load_from_file(m_path, Glib::KeyFile::Flags::KEEP_COMMENTS);
    } catch (const Glib::Error &) {
        // Missing or unreadable: start empty.
    }
}

std::string Settings::string(const char *group, const char *key) const
{
    try {
        return m_file->get_string(group, key);
    } catch (const Glib::Error &) {
        return {};
    }
}

void Settings::setString(const char *group, const char *key, const std::string &value)
{
    m_file->set_string(group, key, value);
    save();
}

void Settings::save()
{
    const std::string directory = Glib::path_get_dirname(m_path);
    g_mkdir_with_parents(directory.c_str(), 0700);
    try {
        m_file->save_to_file(m_path);
    } catch (const Glib::Error &) {
        // Settings are a convenience; failing to persist them is not fatal.
    }
}

std::string Settings::lastSourceFolder() const
{
    return string("files", "lastSourceFolder");
}

void Settings::setLastSourceFolder(const std::string &path)
{
    setString("files", "lastSourceFolder", path);
}

std::string Settings::lastSaveAsFolder() const
{
    return string("files", "lastSaveAsFolder");
}

void Settings::setLastSaveAsFolder(const std::string &path)
{
    setString("files", "lastSaveAsFolder", path);
}

std::vector<Size> Settings::resolutionPresets() const
{
    std::vector<Size> presets;
    try {
        for (const Glib::ustring &value : m_file->get_string_list("display", "presets")) {
            Size size;
            if (parseSize(value, size))
                presets.push_back(size);
        }
    } catch (const Glib::Error &) {
    }
    if (presets.empty())
        presets = {
            {1920, 1080},
            {2560, 1080},
            {2560, 1440},
            {3440, 1440},
            {5120, 1440},
            {3840, 2160},
            {1080, 2560},
            {1440, 3440},
        };
    const Size superUltrawide{5120, 1440};
    if (std::find(presets.begin(), presets.end(), superUltrawide) == presets.end())
        presets.push_back(superUltrawide);
    return presets;
}

void Settings::setResolutionPresets(const std::vector<Size> &presets)
{
    std::vector<Glib::ustring> stored;
    for (const Size &size : presets) {
        if (size.isValid())
            stored.push_back(std::to_string(size.width) + 'x' + std::to_string(size.height));
    }
    m_file->set_string_list("display", "presets", stored);
    save();
}

bool Settings::systemTitleBar() const
{
    try {
        return m_file->get_boolean("window", "systemTitleBar");
    } catch (const Glib::Error &) {
        return false;
    }
}

void Settings::setSystemTitleBar(const bool enabled)
{
    m_file->set_boolean("window", "systemTitleBar", enabled);
    save();
}

} // namespace papercutter
