#include "processing/imageprocessor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <cmath>
#include <filesystem>

namespace papercutter {

bool ImageProcessor::isBackendAvailable()
{
    return !backendExecutable().isEmpty();
}

QString ImageProcessor::backendExecutable()
{
    return QStandardPaths::findExecutable(QStringLiteral("magick"));
}

ProcessingResult ImageProcessor::process(const ProcessingRequest &request) const
{
    const ImageJob &job = request.job;
    ProcessingResult result;
    result.sourcePath = job.sourcePath;
    if (!job.composition.isValid())
        return {false, job.sourcePath, {},
                QStringLiteral("validation"), QStringLiteral("The composition is invalid.")};
    if (!QFileInfo::exists(job.sourcePath))
        return {false, job.sourcePath, {},
                QStringLiteral("validation"), QStringLiteral("The source image does not exist.")};
    if (!isBackendAvailable())
        return {false, job.sourcePath, {},
                QStringLiteral("backend"),
                QStringLiteral("ImageMagick is not installed or magick is not in PATH.")};

    if (request.destinationPath.isEmpty())
        return {false, job.sourcePath, {}, QStringLiteral("validation"),
                QStringLiteral("No Save As destination was selected.")};
    const QString outputPath = QFileInfo(request.destinationPath).absoluteFilePath();
    if (outputPath == QFileInfo(job.sourcePath).absoluteFilePath())
        return {false, job.sourcePath, outputPath, QStringLiteral("validation"),
                QStringLiteral("Save As cannot replace the original image.")};
    if (QFileInfo::exists(outputPath) && !request.overwriteDestination)
        return {false, job.sourcePath, outputPath, QStringLiteral("collision"),
                QStringLiteral("The destination already exists.")};
    QDir outputDirectory = QFileInfo(outputPath).absoluteDir();
    if (!outputDirectory.exists() && !outputDirectory.mkpath(QStringLiteral(".")))
        return {false, job.sourcePath, outputPath, QStringLiteral("destination"),
                QStringLiteral("The destination folder could not be created.")};

    const QFileInfo outputInfo(outputPath);
    const QString outputSuffix = outputInfo.suffix().toLower();
    if (!QSet<QString>{QStringLiteral("jpg"), QStringLiteral("jpeg"),
                       QStringLiteral("png"), QStringLiteral("webp")}.contains(outputSuffix))
        return {false, job.sourcePath, outputPath, QStringLiteral("validation"),
                QStringLiteral("The destination must use JPEG, PNG, or WebP.")};
    const QString temporaryTemplate = outputInfo.absolutePath()
        + QStringLiteral("/.lgl-papercutter-XXXXXX.") + outputInfo.suffix();
    QTemporaryFile temporary(temporaryTemplate);
    if (!temporary.open())
        return {false, job.sourcePath, outputPath, QStringLiteral("temporary-file"),
                QStringLiteral("A temporary output file could not be created.")};
    const QString temporaryPath = temporary.fileName();
    temporary.close();

    const QSize target = job.composition.targetSize;
    const double scale = job.composition.effectiveScaleFor(job.sourceSize);
    const int scaledWidth = std::max(1, qRound(job.sourceSize.width() * scale));
    const int scaledHeight = std::max(1, qRound(job.sourceSize.height() * scale));
    const int travelX = std::max(0, (scaledWidth - target.width()) / 2);
    const int travelY = std::max(0, (scaledHeight - target.height()) / 2);
    const int imageX = (target.width() - scaledWidth) / 2
        + qRound(job.composition.normalizedOffset.x() * travelX);
    const int imageY = (target.height() - scaledHeight) / 2
        + qRound(job.composition.normalizedOffset.y() * travelY);
    const QString geometry = QStringLiteral("%1x%2%3%4")
        .arg(scaledWidth).arg(scaledHeight)
        .arg(imageX >= 0 ? QStringLiteral("+") : QString()).arg(imageX)
        + (imageY >= 0 ? QStringLiteral("+") : QString()) + QString::number(imageY);
    const QString canvasSize = QStringLiteral("%1x%2").arg(target.width()).arg(target.height());
    const QString scaledSize = QStringLiteral("%1x%2!").arg(scaledWidth).arg(scaledHeight);
    QStringList arguments;
    arguments << QStringLiteral("-size") << canvasSize
              << QStringLiteral("xc:black")
              << QStringLiteral("(") << job.sourcePath << QStringLiteral("-auto-orient")
              << QStringLiteral("-filter") << QStringLiteral("Lanczos")
              << QStringLiteral("-resize") << scaledSize << QStringLiteral(")")
              << QStringLiteral("-geometry") << geometry
              << QStringLiteral("-composite");

    if (outputSuffix == QStringLiteral("jpg") || outputSuffix == QStringLiteral("jpeg"))
        arguments << QStringLiteral("-sampling-factor") << QStringLiteral("4:4:4")
                  << QStringLiteral("-quality") << QStringLiteral("95");
    else if (outputSuffix == QStringLiteral("webp"))
        arguments << QStringLiteral("-quality") << QStringLiteral("95");
    else if (outputSuffix == QStringLiteral("png"))
        arguments << QStringLiteral("-define") << QStringLiteral("png:compression-level=9");
    arguments << temporaryPath;

    QProcess process;
    process.setProgram(backendExecutable());
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(5000))
        return {false, job.sourcePath, outputPath, QStringLiteral("render"),
                QStringLiteral("ImageMagick could not be started.")};
    if (!process.waitForFinished(300000)) {
        process.kill();
        process.waitForFinished();
        return {false, job.sourcePath, outputPath, QStringLiteral("render"),
                QStringLiteral("ImageMagick timed out.")};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString diagnostic = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return {false, job.sourcePath, outputPath, QStringLiteral("render"),
                diagnostic.isEmpty() ? QStringLiteral("ImageMagick failed.") : diagnostic};
    }

    QImageReader outputReader(temporaryPath);
    if (outputReader.size() != target)
        return {false, job.sourcePath, outputPath, QStringLiteral("output-validation"),
                QStringLiteral("The rendered image has incorrect dimensions.")};

    QFile::setPermissions(temporaryPath, QFile::permissions(job.sourcePath));
    temporary.setAutoRemove(false);
    std::error_code error;
    std::filesystem::rename(temporaryPath.toStdString(), outputPath.toStdString(), error);
    if (error) {
        QFile::remove(temporaryPath);
        return {false, job.sourcePath, outputPath, QStringLiteral("commit"),
                QStringLiteral("The processed image could not be committed at %1: %2")
                    .arg(outputPath, QString::fromStdString(error.message()))};
    }

    QImageReader committedReader(outputPath);
    if (committedReader.size() != target) {
        return {false, job.sourcePath, outputPath, QStringLiteral("commit-validation"),
                QStringLiteral("The committed image at %1 could not be verified at %2 × %3.")
                    .arg(outputPath)
                    .arg(target.width())
                    .arg(target.height())};
    }
    result.succeeded = true;
    result.outputPath = outputPath;
    return result;
}

} // namespace papercutter
