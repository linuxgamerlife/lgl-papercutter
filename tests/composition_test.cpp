#include "core/composition.h"

#include <glib.h>

using papercutter::CompositionState;

namespace {

void validatesDefaults()
{
    g_assert_true(CompositionState{}.isValid());
}

void rejectsInvalidValues()
{
    CompositionState state;
    state.zoom = 0.0;
    g_assert_false(state.isValid());
    state.zoom = 1.0;
    state.normalizedOffset = {1.1, 0.0};
    g_assert_false(state.isValid());
}

void calculatesCoverScale()
{
    CompositionState state;
    state.targetSize = {3440, 1440};
    g_assert_cmpfloat(state.baseScaleFor({1920, 1080}), ==, 3440.0 / 1920.0);
}

void reportsUpscaling()
{
    CompositionState state;
    state.targetSize = {3840, 2160};
    g_assert_true(state.isUpscaled({1920, 1080}));
    g_assert_false(state.isUpscaled({7680, 4320}));
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/composition/validates-defaults", validatesDefaults);
    g_test_add_func("/composition/rejects-invalid-values", rejectsInvalidValues);
    g_test_add_func("/composition/calculates-cover-scale", calculatesCoverScale);
    g_test_add_func("/composition/reports-upscaling", reportsUpscaling);
    return g_test_run();
}
