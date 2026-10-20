#include "processing/imageprocessor.h"

#include <QFile>
#include <QImage>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>

using papercutter::ImageJob;
using papercutter::ImageProcessor;
using papercutter::ProcessingRequest;

class ImageProcessorTest final : public QObject {
    Q_OBJECT

private slots:
    void packagedBackendOverridesHostDiscovery();
    void fillsTargetWithoutGaps();
    void writesEditedMetadataToExport();
    void lowerJpegQualityReducesFileSize();
    void keepsIndexedPngOutputsPaletteCompressed();
    void savesACopyWithoutChangingTheOriginal();
    void refusesToReplaceTheOriginal();
};

void ImageProcessorTest::packagedBackendOverridesHostDiscovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executablePath = directory.filePath(QStringLiteral("convert"));
    QFile executable(executablePath);
    QVERIFY(executable.open(QIODevice::WriteOnly));
    executable.write("#!/bin/sh\nexit 0\n");
    executable.close();
    QVERIFY(QFile::setPermissions(executablePath,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));

    const bool wasSet = qEnvironmentVariableIsSet("PAPERCUTTER_IMAGEMAGICK");
    const QByteArray previousValue = qgetenv("PAPERCUTTER_IMAGEMAGICK");
    qputenv("PAPERCUTTER_IMAGEMAGICK", executablePath.toUtf8());
    const QString selectedExecutable = ImageProcessor::backendExecutable();
    if (wasSet)
        qputenv("PAPERCUTTER_IMAGEMAGICK", previousValue);
    else
        qunsetenv("PAPERCUTTER_IMAGEMAGICK");

    QCOMPARE(selectedExecutable, executablePath);
}

void ImageProcessorTest::fillsTargetWithoutGaps()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("portrait.png"));
    const QString destinationPath = directory.filePath(QStringLiteral("output.png"));
    QImage source(60, 120, QImage::Format_ARGB32);
    source.fill(Qt::green);
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {160, 90};
    job.composition.normalizedOffset = {0.0, -1.0};
    const ProcessingRequest request{job, destinationPath, false};
    const auto result = ImageProcessor{}.process(request);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(QImage(destinationPath).size(), QSize(160, 90));
    QCOMPARE(QImage(sourcePath).size(), QSize(60, 120));
}

void ImageProcessorTest::writesEditedMetadataToExport()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");
    if (ImageProcessor::metadataExecutable().isEmpty())
        QSKIP("Exiv2 is not available");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.jpg"));
    const QString destinationPath = directory.filePath(QStringLiteral("output.jpg"));
    QImage source(120, 80, QImage::Format_RGB32);
    source.fill(Qt::yellow);
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {60, 40};
    job.metadata.description = QStringLiteral("Mountain sunset");
    job.metadata.artist = QStringLiteral("Test Artist");
    job.metadata.copyright = QStringLiteral("Copyright 2026");
    job.metadata.comment = QStringLiteral("Edited in Papercutter");
    job.metadataEdited = true;

    const auto result = ImageProcessor{}.process({job, destinationPath, false});
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    const auto exportedMetadata = ImageProcessor::readMetadata(destinationPath);
    QCOMPARE(exportedMetadata.description, job.metadata.description);
    QCOMPARE(exportedMetadata.artist, job.metadata.artist);
    QCOMPARE(exportedMetadata.copyright, job.metadata.copyright);
    QCOMPARE(exportedMetadata.comment, job.metadata.comment);
    QVERIFY(ImageProcessor::readMetadata(sourcePath).isEmpty());
}

void ImageProcessorTest::lowerJpegQualityReducesFileSize()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.png"));
    const QString highQualityPath = directory.filePath(QStringLiteral("high.jpg"));
    const QString lowQualityPath = directory.filePath(QStringLiteral("low.jpg"));
    QImage source(512, 512, QImage::Format_RGB32);
    QRandomGenerator random(2026);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x)
            source.setPixel(x, y, random.generate());
    }
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = source.size();
    job.outputQuality = 95;
    const auto highResult = ImageProcessor{}.process({job, highQualityPath, false});
    QVERIFY2(highResult.succeeded, qPrintable(highResult.errorMessage));

    job.outputQuality = 40;
    const auto lowResult = ImageProcessor{}.process({job, lowQualityPath, false});
    QVERIFY2(lowResult.succeeded, qPrintable(lowResult.errorMessage));
    QVERIFY(QFileInfo(lowQualityPath).size() < QFileInfo(highQualityPath).size());
    const qint64 estimatedSize = ImageProcessor::estimateOutputSize(job, QStringLiteral("jpg"));
    QVERIFY(estimatedSize > 0);
    QCOMPARE(estimatedSize, QFileInfo(lowQualityPath).size());
}

void ImageProcessorTest::keepsIndexedPngOutputsPaletteCompressed()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("indexed.png"));
    const QString destinationPath = directory.filePath(QStringLiteral("output.png"));
    QImage source(512, 256, QImage::Format_Indexed8);
    QVector<QRgb> palette;
    palette.reserve(256);
    for (int value = 0; value < 256; ++value)
        palette.push_back(qRgb(value, (value * 3) % 256, (value * 7) % 256));
    source.setColorTable(palette);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x)
            source.setPixel(x, y, (x * 17 + y * 31) % 256);
    }
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {800, 256};
    job.composition.zoom = 1.001 / job.composition.baseScaleFor(job.sourceSize);
    const auto result = ImageProcessor{}.process({job, destinationPath, false});
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));

    const QImage output(destinationPath);
    QCOMPARE(output.size(), job.composition.targetSize);
    QFile outputFile(destinationPath);
    QVERIFY(outputFile.open(QIODevice::ReadOnly));
    const QByteArray header = outputFile.read(26);
    QCOMPARE(header.size(), 26);
    QCOMPARE(static_cast<unsigned char>(header.at(25)), static_cast<unsigned char>(3));
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
    const ProcessingRequest request{job, destinationPath, false};
    const auto result = ImageProcessor{}.process(request);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(QImage(sourcePath).size(), QSize(120, 80));
    QCOMPARE(QImage(destinationPath).size(), QSize(60, 100));
}

void ImageProcessorTest::refusesToReplaceTheOriginal()
{
    if (!ImageProcessor::isBackendAvailable())
        QSKIP("ImageMagick is not available");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("original.png"));
    QImage source(120, 80, QImage::Format_ARGB32);
    source.fill(Qt::blue);
    QVERIFY(source.save(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = source.size();
    job.composition.targetSize = {60, 40};
    const auto result = ImageProcessor{}.process({job, sourcePath, true});
    QVERIFY(!result.succeeded);
    QCOMPARE(result.failureStage, QStringLiteral("validation"));
    QCOMPARE(QImage(sourcePath).size(), QSize(120, 80));
}

QTEST_MAIN(ImageProcessorTest)
#include "imageprocessor_test.moc"
