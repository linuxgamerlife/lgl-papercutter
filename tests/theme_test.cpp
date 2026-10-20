#include "core/theme.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace papercutter;

namespace {

struct TemporaryConfig {
    std::string path;

    TemporaryConfig()
    {
        gchar *created = g_dir_make_tmp("papercutter-theme-XXXXXX", nullptr);
        g_assert_nonnull(created);
        path = created;
        g_free(created);
        std::filesystem::create_directories(path + "/gtk-4.0");
    }
    ~TemporaryConfig()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    void write(const std::string &name, const std::string &text) const
    {
        std::ofstream(path + "/gtk-4.0/" + name) << text;
    }
};

void followsImports()
{
    TemporaryConfig config;
    config.write("gtk.css",
                 "/* @import url('ignored.css'); */\n"
                 "@import url('palette.css');\n"
                 "@import \"https://example.com/remote.css\";\n"
                 "@import url(\"missing.css\");\n");
    config.write("palette.css", "@import 'gtk.css';\n@define-color window_bg_color #000000;\n");
    config.write("ignored.css", "");
    const auto paths = theme::gtkCssPaths(config.path);
    g_assert_cmpuint(paths.size(), ==, 2);
    g_assert_true(paths[0].find("gtk.css") != std::string::npos);
    g_assert_true(paths[1].find("palette.css") != std::string::npos);
}

void resolvesNamedColors()
{
    TemporaryConfig config;
    config.write("gtk.css",
                 "@define-color accent_bg_color #3584e4;\n"
                 "@define-color theme_selected_bg_color @accent_bg_color;\n"
                 "@define-color window_bg_color rgb(30, 30, 30);\n"
                 "@define-color shaded alpha(@accent_bg_color, 0.5);\n"
                 "@define-color loop_a @loop_b;\n@define-color loop_b @loop_a;\n");
    const auto colors = theme::gtkColors(theme::gtkCssPaths(config.path));
    g_assert_true(colors.at("theme_selected_bg_color") == "#3584e4");
    g_assert_true(colors.at("window_bg_color") == "rgb(30, 30, 30)");
    g_assert_true(colors.count("shaded") == 0);
    g_assert_true(colors.count("loop_a") == 0);
}

void extractsFontFamilies()
{
    g_assert_true(theme::fontFamily("Adwaita Sans Bold 11") == "Adwaita Sans");
    g_assert_true(theme::fontFamily("JetBrains Mono 10.5") == "JetBrains Mono");
    g_assert_true(theme::fontFamily("Noto Sans Semi-Bold Italic 12px") == "Noto Sans");
    g_assert_true(theme::fontFamily("") == "");
}

void detectsDarkBackgrounds()
{
    g_assert_cmpfloat_with_epsilon(*theme::luminance("#fff"), 1.0, 1e-9);
    g_assert_cmpfloat_with_epsilon(*theme::luminance("rgba(0, 0, 0, 0.5)"), 0.0, 1e-9);
    g_assert_false(theme::luminance("transparent").has_value());
    g_assert_true(theme::isDark({{"window_bg_color", "#1e1e1e"}}, false));
    g_assert_false(theme::isDark({{"theme_bg_color", "#fafafa"}}, true));
    g_assert_true(theme::isDark({}, true));
}

void buildsFontStylesheet()
{
    theme::Theme current;
    g_assert_true(theme::fontStylesheet(current).find("font-family: \"Adwaita Mono\"")
                  != std::string::npos);
    current.monoFont = "Fira \"Code\"";
    g_assert_true(theme::fontStylesheet(current).find("\"Fira Code\", \"Adwaita Mono\"")
                  != std::string::npos);
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/theme/follows-imports", followsImports);
    g_test_add_func("/theme/resolves-named-colors", resolvesNamedColors);
    g_test_add_func("/theme/extracts-font-families", extractsFontFamilies);
    g_test_add_func("/theme/detects-dark-backgrounds", detectsDarkBackgrounds);
    g_test_add_func("/theme/builds-font-stylesheet", buildsFontStylesheet);
    return g_test_run();
}
