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
    result.operation = request.operation;
    result.sourcePath = job.sourcePath;
    if (!job.composition.isValid())
        return {false, request.operation, job.sourcePath, {}, {}, {},
                QStringLiteral("validation"), QStringLiteral("The composition is invalid.")};
    if (!QFileInfo::exists(job.sourcePath))
        return {false, request.operation, job.sourcePath, {}, {}, {},
                QStringLiteral("validation"), QStringLiteral("The source image does not exist.")};
    if (!isBackendAvailable())
        return {false, request.operation, job.sourcePath, {}, {}, {},
                QStringLiteral("backend"),
                QStringLiteral("ImageMagick is not installed or magick is not in PATH.")};

    const QFileInfo source(job.sourcePath);
    QString outputPath = request.operation == ProcessingOperation::AcceptAndReplace
        ? source.absoluteFilePath() : QFileInfo(request.destinationPath).absoluteFilePath();
    if (request.operation == ProcessingOperation::SaveAs) {
        if (request.destinationPath.isEmpty())
            return {false, request.operation, job.sourcePath, {}, {}, {},
                    QStringLiteral("validation"), QStringLiteral("No Save As destination was selected.")};
        if (QFileInfo::exists(outputPath) && !request.overwriteDestination)
            return {false, request.operation, job.sourcePath, outputPath, {}, {},
                    QStringLiteral("collision"), QStringLiteral("The destination already exists.")};
        QDir outputDirectory = QFileInfo(outputPath).absoluteDir();
        if (!outputDirectory.exists() && !outputDirectory.mkpath(QStringLiteral(".")))
            return {false, request.operation, job.sourcePath, outputPath, {}, {},
                    QStringLiteral("destination"),
                    QStringLiteral("The destination folder could not be created.")};
    } else {
        const BackupResult backup = BackupService(request.backupFolder).createBackup(
            job.sourcePath, job.composition.targetSize);
        if (!backup.succeeded)
            return {false, request.operation, job.sourcePath, {}, {}, {},
                    QStringLiteral("backup"), backup.errorMessage};
        result.backupRecord = backup.record;
        result.backupPath = backup.record.backupPath;
    }

    const QFileInfo outputInfo(outputPath);
    const QString outputSuffix = outputInfo.suffix().toLower();
    if (!QSet<QString>{QStringLiteral("jpg"), QStringLiteral("jpeg"),
                       QStringLiteral("png"), QStringLiteral("webp")}.contains(outputSuffix))
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("validation"),
                QStringLiteral("The destination must use JPEG, PNG, or WebP.")};
    const QString temporaryTemplate = outputInfo.absolutePath()
        + QStringLiteral("/.lgl-papercutter-XXXXXX.") + outputInfo.suffix();
    QTemporaryFile temporary(temporaryTemplate);
    if (!temporary.open())
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("temporary-file"),
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
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("render"),
                QStringLiteral("ImageMagick could not be started.")};
    if (!process.waitForFinished(300000)) {
        process.kill();
        process.waitForFinished();
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("render"),
                QStringLiteral("ImageMagick timed out.")};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString diagnostic = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("render"),
                diagnostic.isEmpty() ? QStringLiteral("ImageMagick failed.") : diagnostic};
    }

    QImageReader outputReader(temporaryPath);
    if (outputReader.size() != target)
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("output-validation"),
                QStringLiteral("The rendered image has incorrect dimensions.")};

    QFile::setPermissions(temporaryPath, QFile::permissions(job.sourcePath));
    temporary.setAutoRemove(false);
    std::error_code error;
    std::filesystem::rename(temporaryPath.toStdString(), outputPath.toStdString(), error);
    if (error) {
        QFile::remove(temporaryPath);
        return {false, request.operation, job.sourcePath, outputPath, result.backupPath,
                result.backupRecord, QStringLiteral("commit"),
                QStringLiteral("The processed image could not replace the source: %1")
                    .arg(QString::fromStdString(error.message()))};
    }
    result.succeeded = true;
    result.outputPath = outputPath;
    return result;
}

} // namespace papercutter
