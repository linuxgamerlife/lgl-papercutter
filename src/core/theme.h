#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

// Port of lgl-distro-desktop-score app/theme.py: reads the desktop's GTK 4 palette
// (`@define-color` rules in ~/.config/gtk-4.0/gtk.css and its @imports, as written by
// generators such as Noctalia) to find the files to watch and whether the palette is dark,
// and builds the monospace font stylesheet. Pure functions so they can be tested without a
// display.
namespace papercutter::theme {

using Colors = std::map<std::string, std::string>;

struct Theme {
    Colors colors;
    bool dark{false};
    std::string monoFont;
};

// The user GTK 4 stylesheet and every local file it imports, in load order.
std::vector<std::string> gtkCssPaths(const std::string &configDir);

// Named colours from `@define-color` rules with `@name` references resolved. Only values
// that are plain CSS colours (#hex, rgb(), rgba()) are kept.
Colors gtkColors(const std::vector<std::string> &paths);

// The family from a Pango font description such as "Adwaita Sans Bold 11".
std::string fontFamily(const std::string &description);

// Relative luminance (0–1) of a #hex or rgb()/rgba() colour.
std::optional<double> luminance(const std::string &color);

// Dark when the window background is dark; otherwise follow the desktop preference.
bool isDark(const Colors &colors, bool preferDark);

// Font-family rules for the monospace classes, preferring the desktop monospace font.
std::string fontStylesheet(const Theme &theme);

} // namespace papercutter::theme
