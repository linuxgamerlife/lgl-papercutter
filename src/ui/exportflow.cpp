#include "ui/exportflow.h"

#include "core/numbering.h"
#include "core/processor.h"
#include "ui/dialogs.h"

#include <giomm/file.h>
#include <giomm/liststore.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/main.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>

#include <set>

namespace papercutter {

namespace {

std::string absolutePath(const std::string &path)
{
    return Gio::File::create_for_path(path)->get_path();
}

std::string joinLines(const std::vector<std::string> &lines)
{
    std::string text;
    for (const std::string &line : lines) {
        if (!text.empty())
            text += '\n';
        text += line;
    }
    return text;
}

Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> imageFilters()
{
    auto images = Gtk::FileFilter::create();
    images->set_name("Images (*.jpg *.jpeg *.png *.webp)");
    for (const char *suffix : {"jpg", "jpeg", "png", "webp"})
        images->add_suffix(suffix);
    auto filters = Gio::ListStore<Gtk::FileFilter>::create();
    filters->append(images);
    return filters;
}

// Pops the next free number into a file name, keeping suffix as typed.
bool takeNumberedName(DestinationNumbering &numbering, const std::string &suffix,
                      std::string &name)
{
    unsigned long long number = 0;
    if (!takeNextAvailableNumber(numbering, number))
        return false;
    name = numberedFileName(number, numbering.paddingWidth, suffix);
    return true;
}

} // namespace

struct ExportFlow::Batch {
    std::vector<ImageJob> jobs;
    std::string folder;
    DestinationNumbering numbering;
    bool sequential{false};
    std::vector<ProcessingRequest> requests;
    std::vector<std::string> skipped;
    std::set<std::string> plannedDestinations;
    std::vector<ProcessingResult> results;
};

ExportFlow::ExportFlow(Gtk::Window &parent, Settings &settings, Notify notify)
    : m_parent(parent)
    , m_settings(settings)
    , m_notify(std::move(notify))
{
}

void ExportFlow::chooseNumbering(const std::string &folder,
                                 std::function<void(bool, bool)> done)
{
    DestinationNumbering preview = inspectDestinationNumbering(folder);
    const DestinationNumbering numbering = preview;
    unsigned long long firstAvailable = 0;
    if (!numbering.foundNumberedFile || !takeNextAvailableNumber(preview, firstAvailable)) {
        done(true, false);
        return;
    }
    const std::string highest = paddedNumber(numbering.highestNumber, numbering.paddingWidth);
    const std::string first = paddedNumber(firstAvailable, numbering.paddingWidth);
    const std::string summary = firstAvailable <= numbering.highestNumber
        ? "The highest numbered file is " + highest + ", and the first gap is " + first + "."
        : "The highest numbered file in this folder is " + highest + ".";
    askChoice(m_parent, "File numbering",
              summary + "\n\nNumber exported files sequentially, starting at " + first
                  + "? You will see the exact file names before anything is saved.",
              {"Cancel", "Keep Names", "Number Sequentially"}, 0, 2,
              [done = std::move(done)](const int answer) {
                  if (answer == 0)
                      done(false, false);
                  else
                      done(true, answer == 2);
              });
}

void ExportFlow::saveSingle(const ImageJob &job)
{
    const std::string initialFolder = m_settings.lastSaveAsFolder().empty()
        ? Glib::path_get_dirname(job.sourcePath) : m_settings.lastSaveAsFolder();
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Save processed image as");
    dialog->set_modal(true);
    dialog->set_initial_folder(Gio::File::create_for_path(initialFolder));
    dialog->set_initial_name(Glib::path_get_basename(job.sourcePath));
    dialog->set_filters(imageFilters());
    dialog->save(m_parent, [this, dialog, job](Glib::RefPtr<Gio::AsyncResult> &result) {
        Glib::RefPtr<Gio::File> file;
        try {
            file = dialog->save_finish(result);
        } catch (const Glib::Error &) {
            return;
        }
        if (!file || file->get_path().empty())
            return;
        continueSingle(job, file->get_path());
    });
}

void ExportFlow::continueSingle(const ImageJob &job, std::string destination)
{
    const std::string folder = Glib::path_get_dirname(destination);
    chooseNumbering(folder, [this, job, destination, folder](const bool proceed,
                                                              const bool sequential) mutable {
        if (!proceed)
            return;
        if (sequential) {
            DestinationNumbering numbering = inspectDestinationNumbering(folder);
            std::string name;
            if (!takeNumberedName(numbering, rawFileSuffix(destination), name)) {
                showMessage(m_parent, "File numbering unavailable",
                            "No numeric filename is available.");
                return;
            }
            destination = Glib::build_filename(folder, name);
        }
        if (absolutePath(destination) == absolutePath(job.sourcePath)) {
            showMessage(m_parent, "Choose another destination",
                        "Save As never replaces the original image. Choose a different "
                        "filename or folder.");
            return;
        }

        auto confirmAndRun = [this, job, destination](const bool overwrite) {
            const ProcessingRequest request{job, destination, overwrite};
            confirmExportMappings(m_parent, {request}, [this, request](const bool accepted) {
                if (!accepted)
                    return;
                m_notify("Saving copy…", NoticeKind::Info, 0);
                ImageProcessor::processAsync(request, [this](const ProcessingResult &result) {
                    if (!result.succeeded) {
                        showMessage(m_parent, "Save As failed", result.errorMessage);
                        m_notify("Save As failed.", NoticeKind::Negative, 5000);
                        return;
                    }
                    m_settings.setLastSaveAsFolder(Glib::path_get_dirname(result.outputPath));
                    m_notify("Saved " + result.outputPath, NoticeKind::Positive, 10000);
                });
            });
        };
        if (!Glib::file_test(destination, Glib::FileTest::EXISTS)) {
            confirmAndRun(false);
            return;
        }
        askChoice(m_parent, "Replace existing file?", destination + " already exists. Replace it?",
                  {"Cancel", "Replace"}, 0, 0, [confirmAndRun](const int answer) {
                      if (answer == 1)
                          confirmAndRun(true);
                  });
    });
}

void ExportFlow::saveBatch(const std::vector<ImageJob> &jobs)
{
    if (jobs.empty())
        return;
    if (jobs.size() == 1) {
        saveSingle(jobs.front());
        return;
    }
    const std::string initialFolder = m_settings.lastSaveAsFolder().empty()
        ? Glib::path_get_dirname(jobs.front().sourcePath) : m_settings.lastSaveAsFolder();
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Choose export folder");
    dialog->set_modal(true);
    dialog->set_initial_folder(Gio::File::create_for_path(initialFolder));
    dialog->select_folder(m_parent, [this, dialog, jobs](Glib::RefPtr<Gio::AsyncResult> &result) {
        Glib::RefPtr<Gio::File> file;
        try {
            file = dialog->select_folder_finish(result);
        } catch (const Glib::Error &) {
            return;
        }
        if (!file || file->get_path().empty())
            return;
        auto batch = std::make_shared<Batch>();
        batch->jobs = jobs;
        batch->folder = file->get_path();
        chooseNumbering(batch->folder, [this, batch](const bool proceed, const bool sequential) {
            if (!proceed)
                return;
            batch->sequential = sequential;
            batch->numbering = inspectDestinationNumbering(batch->folder);
            planBatch(batch, 0);
        });
    });
}

void ExportFlow::planBatch(const std::shared_ptr<Batch> &batch, std::size_t index)
{
    for (; index < batch->jobs.size(); ++index) {
        const ImageJob &job = batch->jobs[index];
        const std::string sourceName = Glib::path_get_basename(job.sourcePath);
        std::string outputName = sourceName;
        if (batch->sequential
            && !takeNumberedName(batch->numbering, rawFileSuffix(job.sourcePath), outputName)) {
            batch->skipped.push_back(sourceName + ": destination numbering is exhausted");
            continue;
        }
        const std::string destination = Glib::build_filename(batch->folder, outputName);
        const std::string absoluteDestination = absolutePath(destination);
        if (absoluteDestination == absolutePath(job.sourcePath)) {
            batch->skipped.push_back(sourceName + ": destination is the original source");
            continue;
        }
        if (Glib::file_test(destination, Glib::FileTest::EXISTS)
            || batch->plannedDestinations.count(absoluteDestination) != 0) {
            askChoice(m_parent, "Replace existing file?",
                      destination + " conflicts with another export.\n\n"
                                    "Replace it, skip this image, or cancel the remaining batch?",
                      {"Cancel Batch", "Skip", "Replace"}, 0, 1,
                      [this, batch, index, destination, absoluteDestination,
                       sourceName](const int answer) {
                          if (answer == 0) {
                              batch->skipped.push_back("Remaining exports were cancelled.");
                              confirmBatch(batch);
                              return;
                          }
                          if (answer == 1) {
                              batch->skipped.push_back(sourceName
                                                       + ": conflicting destination skipped");
                          } else {
                              batch->requests.push_back(
                                  {batch->jobs[index], destination, true});
                              batch->plannedDestinations.insert(absoluteDestination);
                          }
                          planBatch(batch, index + 1);
                      });
            return;
        }
        batch->requests.push_back({job, destination, false});
        batch->plannedDestinations.insert(absoluteDestination);
    }
    confirmBatch(batch);
}

void ExportFlow::confirmBatch(const std::shared_ptr<Batch> &batch)
{
    if (batch->requests.empty()) {
        if (!batch->skipped.empty())
            showMessage(m_parent, "Nothing exported", joinLines(batch->skipped));
        return;
    }
    confirmExportMappings(m_parent, batch->requests, [this, batch](const bool accepted) {
        if (accepted)
            runBatch(batch, 0);
    });
}

void ExportFlow::runBatch(const std::shared_ptr<Batch> &batch, const std::size_t index)
{
    const std::size_t total = batch->requests.size();
    if (index < total) {
        m_notify("Saving " + std::to_string(index + 1) + " of " + std::to_string(total)
                     + " images…",
                 NoticeKind::Info, 0);
        ImageProcessor::processAsync(batch->requests[index],
                                     [this, batch, index](const ProcessingResult &result) {
            batch->results.push_back(result);
            // Continue from the main loop rather than recursing inside the callback.
            Glib::signal_idle().connect_once([this, batch, index] { runBatch(batch, index + 1); });
        });
        return;
    }

    int succeeded = 0;
    std::vector<std::string> report = batch->skipped;
    for (const ProcessingResult &result : batch->results) {
        if (result.succeeded)
            ++succeeded;
        else
            report.push_back(Glib::path_get_basename(result.sourcePath) + ": "
                             + result.errorMessage);
    }
    m_settings.setLastSaveAsFolder(batch->folder);
    m_notify("Saved " + std::to_string(succeeded) + " of " + std::to_string(total)
                 + " selected image(s).",
             succeeded == static_cast<int>(total) ? NoticeKind::Positive : NoticeKind::Neutral,
             10000);
    if (!report.empty())
        showMessage(m_parent, "Batch export report", joinLines(report));
}

} // namespace papercutter
