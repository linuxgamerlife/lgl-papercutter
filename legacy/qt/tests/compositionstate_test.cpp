#include "core/compositionstate.h"

#include <QTest>

using papercutter::CompositionState;

class CompositionStateTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesDefaults();
    void rejectsInvalidValues();
    void calculatesCoverScale();
    void reportsUpscaling();
};

void CompositionStateTest::validatesDefaults()
{
    QVERIFY(CompositionState{}.isValid());
}

void CompositionStateTest::rejectsInvalidValues()
{
    CompositionState state;
    state.zoom = 0.0;
    QVERIFY(!state.isValid());
    state.zoom = 1.0;
    state.normalizedOffset = {1.1, 0.0};
    QVERIFY(!state.isValid());
}

void CompositionStateTest::calculatesCoverScale()
{
    CompositionState state;
    state.targetSize = {3440, 1440};
    QCOMPARE(state.baseScaleFor({1920, 1080}), 3440.0 / 1920.0);
}

void CompositionStateTest::reportsUpscaling()
{
    CompositionState state;
    state.targetSize = {3840, 2160};
    QVERIFY(state.isUpscaled({1920, 1080}));
    QVERIFY(!state.isUpscaled({7680, 4320}));
}

QTEST_MAIN(CompositionStateTest)
#include "compositionstate_test.moc"
