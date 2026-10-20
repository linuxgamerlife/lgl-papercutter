#include "core/settings.h"

#include <glib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using namespace papercutter;

namespace {

struct TemporaryConfig {
    std::string folder;
    std::string path;

    TemporaryConfig()
    {
        gchar *created = g_dir_make_tmp("papercutter-settings-XXXXXX", nullptr);
        g_assert_nonnull(created);
        folder = created;
        g_free(created);
        path = folder + "/lgl-papercutter/settings.ini";
    }
    ~TemporaryConfig()
    {
        std::error_code error;
        std::filesystem::remove_all(folder, error);
    }
    void write(const std::string &contents) const
    {
        std::filesystem::create_directories(folder + "/lgl-papercutter");
        std::ofstream(path) << contents;
    }
};

void missingFileUsesDefaults()
{
    TemporaryConfig config;
    const Settings settings(config.path);
    g_assert_false(settings.systemTitleBar());
    g_assert_true(settings.lastSourceFolder().empty());
    g_assert_true(settings.lastSaveAsFolder().empty());
    const auto presets = settings.resolutionPresets();
    g_assert_true(std::find(presets.begin(), presets.end(), Size{1920, 1080}) != presets.end());
    g_assert_false(std::filesystem::exists(config.path));
}

void systemTitleBarPersists()
{
    TemporaryConfig config;
    Settings(config.path).setSystemTitleBar(true);
    g_assert_true(std::filesystem::exists(config.path));
    g_assert_true(Settings(config.path).systemTitleBar());
    Settings(config.path).setSystemTitleBar(false);
    g_assert_false(Settings(config.path).systemTitleBar());
}

void foldersPersist()
{
    TemporaryConfig config;
    {
        Settings settings(config.path);
        settings.setLastSourceFolder("/home/user/Pictures");
        settings.setLastSaveAsFolder("/home/user/Wallpapers");
    }
    const Settings settings(config.path);
    const std::string source = settings.lastSourceFolder();
    const std::string saveAs = settings.lastSaveAsFolder();
    g_assert_cmpstr(source.c_str(), ==, "/home/user/Pictures");
    g_assert_cmpstr(saveAs.c_str(), ==, "/home/user/Wallpapers");
}

void presetsPersistAndKeepSuperUltrawide()
{
    TemporaryConfig config;
    Settings(config.path).setResolutionPresets({{1920, 1080}, {0, 720}});
    const auto presets = Settings(config.path).resolutionPresets();
    g_assert_cmpuint(presets.size(), ==, 2);
    g_assert_true(presets[0] == (Size{1920, 1080}));
    g_assert_true(presets[1] == (Size{5120, 1440}));
}

void malformedValuesUseDefaults()
{
    TemporaryConfig config;
    config.write("[window]\nsystemTitleBar=maybe\n[display]\npresets=wide;1920x;-5x10;\n");
    const Settings settings(config.path);
    g_assert_false(settings.systemTitleBar());
    const auto presets = settings.resolutionPresets();
    g_assert_cmpuint(presets.size(), >, 2);
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/settings/missing-file-uses-defaults", missingFileUsesDefaults);
    g_test_add_func("/settings/system-title-bar-persists", systemTitleBarPersists);
    g_test_add_func("/settings/folders-persist", foldersPersist);
    g_test_add_func("/settings/presets-persist-and-keep-super-ultrawide",
                    presetsPersistAndKeepSuperUltrawide);
    g_test_add_func("/settings/malformed-values-use-defaults", malformedValuesUseDefaults);
    return g_test_run();
}
