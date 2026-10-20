#pragma once

#include "core/processor.h"

#include <gtkmm/window.h>

#include <functional>
#include <string>
#include <vector>

namespace papercutter {

// A message with a single Close button.
void showMessage(Gtk::Window &parent, const std::string &heading, const std::string &detail);

// Buttons are shown in order; done receives the chosen index, or cancelButton when the
// dialog is dismissed.
void askChoice(Gtk::Window &parent, const std::string &heading, const std::string &detail,
               const std::vector<Glib::ustring> &buttons, int cancelButton, int defaultButton,
               std::function<void(int)> done);

// Shows every source → destination pair before an export starts; done(true) to save.
void confirmExportMappings(Gtk::Window &parent, const std::vector<ProcessingRequest> &requests,
                           std::function<void(bool)> done);

} // namespace papercutter
