#include "processing/imageprocessor.h"
#include "app/backupservice.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using papercutter::ImageJob;
using papercutter::ImageProcessor;
using papercutter::ProcessingOperation;
using papercutter::ProcessingRequest;

class ImageProcessorTest final : public QObject {
    Q_OBJECT

private slots:
    void backsUpAndReplacesImage();
    void fillsTargetWithoutGaps();
    void savesACopyWithoutChangingTheOriginal();
    void restoresAVerifiedBackup();
};

void ImageProcessorTest::backsUpAndReplacesImage()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.png"));
    const QString backupFolder = directory.filePath(QStringLiteral("backups"));

    QImage source(200, 200, QImage::Format_ARGB32);
    source.fill(Qt::blue);
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {100, 50};

    ProcessingRequest request;
    request.job = job;
    request.operation = ProcessingOperation::AcceptAndReplace;
    request.backupFolder = backupFolder;
    const auto result = ImageProcessor{}.process(request);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QVERIFY(QFileInfo::exists(result.backupPath));
    QCOMPARE(QImage(result.backupPath).size(), QSize(200, 200));
    QCOMPARE(QImage(sourcePath).size(), QSize(100, 50));
}

void ImageProcessorTest::fillsTargetWithoutGaps()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("portrait.png"));

    QImage source(60, 120, QImage::Format_ARGB32);
    source.fill(Qt::green);
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {160, 90};
    job.composition.normalizedOffset = {0.0, -1.0};

    ProcessingRequest request;
    request.job = job;
    request.operation = ProcessingOperation::AcceptAndReplace;
    request.backupFolder = directory.filePath(QStringLiteral("backups"));
    const auto result = ImageProcessor{}.process(request);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(QImage(sourcePath).size(), QSize(160, 90));
}

void ImageProcessorTest::savesACopyWithoutChangingTheOriginal()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("wallpaper.png"));
    const QString destinationPath = directory.filePath(QStringLiteral("exports/wallpaper.png"));
    QImage source(120, 80, QImage::Format_ARGB32);
    source.fill(Qt::red);
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {60, 100};
    ProcessingRequest request;
    request.job = job;
    request.operation = ProcessingOperation::SaveAs;
    request.destinationPath = destinationPath;

    const auto result = ImageProcessor{}.process(request);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(QImage(sourcePath).size(), QSize(120, 80));
    QCOMPARE(QImage(destinationPath).size(), QSize(60, 100));
    QVERIFY(result.backupPath.isEmpty());
}

void ImageProcessorTest::restoresAVerifiedBackup()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("original.png"));
    const QString backupFolder = directory.filePath(QStringLiteral("backups"));
    QImage source(200, 100, QImage::Format_ARGB32);
    source.fill(Qt::yellow);
    QVERIFY(source.save(sourcePath));
    const QString originalHash = papercutter::BackupService::fileSha256(sourcePath);

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {80, 80};
    ProcessingRequest request;
    request.job = job;
    request.operation = ProcessingOperation::AcceptAndReplace;
    request.backupFolder = backupFolder;
    const auto processResult = ImageProcessor{}.process(request);
    QVERIFY2(processResult.succeeded, qPrintable(processResult.errorMessage));

    papercutter::BackupService backups(backupFolder);
    const auto restoreResult = backups.restore(processResult.backupRecord);
    QVERIFY2(restoreResult.succeeded, qPrintable(restoreResult.errorMessage));
    QCOMPARE(QImage(sourcePath).size(), QSize(200, 100));
    QCOMPARE(papercutter::BackupService::fileSha256(sourcePath), originalHash);
    QVERIFY(QFileInfo::exists(restoreResult.recoveryBackup.backupPath));
    QCOMPARE(backups.records().size(), 2);
}

QTEST_MAIN(ImageProcessorTest)
#include "imageprocessor_test.moc"
