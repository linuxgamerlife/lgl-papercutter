#include "core/numbering.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace papercutter;

namespace {

struct TemporaryFolder {
    std::string path;

    TemporaryFolder()
    {
        gchar *created = g_dir_make_tmp("papercutter-numbering-XXXXXX", nullptr);
        g_assert_nonnull(created);
        path = created;
        g_free(created);
    }
    ~TemporaryFolder()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    void touch(const std::string &name) const
    {
        std::ofstream(path + "/" + name) << "x";
    }
};

std::vector<unsigned long long> take(DestinationNumbering &numbering, int count)
{
    std::vector<unsigned long long> numbers;
    for (int i = 0; i < count; ++i) {
        unsigned long long number = 0;
        g_assert_true(takeNextAvailableNumber(numbering, number));
        numbers.push_back(number);
    }
    return numbers;
}

void fillsGapsBeforeContinuing()
{
    TemporaryFolder folder;
    folder.touch("0312.jpg");
    folder.touch("0314.png");
    folder.touch("0317.webp");
    DestinationNumbering numbering = inspectDestinationNumbering(folder.path);
    g_assert_true(numbering.foundNumberedFile);
    g_assert_cmpuint(numbering.lowestNumber, ==, 312);
    g_assert_cmpuint(numbering.highestNumber, ==, 317);
    g_assert_cmpuint(numbering.paddingWidth, ==, 4);
    const auto numbers = take(numbering, 4);
    g_assert_cmpuint(numbers[0], ==, 313);
    g_assert_cmpuint(numbers[1], ==, 315);
    g_assert_cmpuint(numbers[2], ==, 316);
    g_assert_cmpuint(numbers[3], ==, 318);
}

void ignoresNonNumericEntries()
{
    TemporaryFolder folder;
    folder.touch("notes.txt");
    folder.touch("abc123.jpg");
    folder.touch("0313x.jpg");
    folder.touch(".5.jpg");
    folder.touch("99999999999999999999999.jpg");
    g_mkdir((folder.path + "/12").c_str(), 0700);
    DestinationNumbering numbering = inspectDestinationNumbering(folder.path);
    g_assert_false(numbering.foundNumberedFile);
    unsigned long long number = 0;
    g_assert_false(takeNextAvailableNumber(numbering, number));
}

void keepsWidestPadding()
{
    TemporaryFolder folder;
    folder.touch("007.jpg");
    folder.touch("9.png");
    DestinationNumbering numbering = inspectDestinationNumbering(folder.path);
    g_assert_cmpuint(numbering.paddingWidth, ==, 3);
    unsigned long long number = 0;
    g_assert_true(takeNextAvailableNumber(numbering, number));
    g_assert_cmpuint(number, ==, 8);
    g_assert_true(numberedFileName(number, numbering.paddingWidth, "JPG") == "008.JPG");
    g_assert_true(numberedFileName(number, numbering.paddingWidth, "") == "008");
    g_assert_true(paddedNumber(12345, 3) == "12345");
}

void usesCompleteBaseName()
{
    TemporaryFolder folder;
    folder.touch("42.tar.gz");
    folder.touch("41");
    DestinationNumbering numbering = inspectDestinationNumbering(folder.path);
    g_assert_true(numbering.foundNumberedFile);
    g_assert_cmpuint(numbering.lowestNumber, ==, 41);
    g_assert_cmpuint(numbering.highestNumber, ==, 41);
}

void missingFolderHasNoNumbers()
{
    const DestinationNumbering numbering =
        inspectDestinationNumbering("/nonexistent/papercutter/folder");
    g_assert_false(numbering.foundNumberedFile);
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/numbering/fills-gaps-before-continuing", fillsGapsBeforeContinuing);
    g_test_add_func("/numbering/ignores-non-numeric-entries", ignoresNonNumericEntries);
    g_test_add_func("/numbering/keeps-widest-padding", keepsWidestPadding);
    g_test_add_func("/numbering/uses-complete-base-name", usesCompleteBaseName);
    g_test_add_func("/numbering/missing-folder-has-no-numbers", missingFolderHasNoNumbers);
    return g_test_run();
}
