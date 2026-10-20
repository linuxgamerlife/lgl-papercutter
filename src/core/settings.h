#pragma once

#include "core/composition.h"

#include <glibmm/keyfile.h>

#include <string>
#include <vector>

namespace papercutter {

// Port of the Qt SettingsService on a GKeyFile stored in
// $XDG_CONFIG_HOME/lgl-papercutter/settings.ini (the Qt app uses QSettings elsewhere).
// Every setter saves immediately.
class Settings final {
public:
    Settings();
    explicit Settings(std::string path);

    std::string lastSourceFolder() const;
    void setLastSourceFolder(const std::string &path);

    std::string lastSaveAsFolder() const;
    void setLastSaveAsFolder(const std::string &path);

    std::vector<Size> resolutionPresets() const;
    void setResolutionPresets(const std::vector<Size> &presets);

    // Whether the compositor draws the title bar (server-side decorations) and the header
    // bar sits inside the window instead.
    bool systemTitleBar() const;
    void setSystemTitleBar(bool enabled);

    const std::string &path() const { return m_path; }

private:
    std::string string(const char *group, const char *key) const;
    void setString(const char *group, const char *key, const std::string &value);
    void save();

    std::string m_path;
    Glib::RefPtr<Glib::KeyFile> m_file;
};

} // namespace papercutter
