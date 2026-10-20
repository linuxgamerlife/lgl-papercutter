#include "processing/imageprocessor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <cmath>
#include <filesystem>

namespace papercutter {

namespace {

struct MetadataField {
    const char *key;
    QString ImageMetadata::*member;
};

constexpr MetadataField metadataFields[] = {
    {"Exif.Image.ImageDescription", &ImageMetadata::description},
    {"Exif.Image.Artist", &ImageMetadata::artist},
    {"Exif.Image.Copyright", &ImageMetadata::copyright},
    {"Exif.Photo.UserComment", &ImageMetadata::comment},
};

bool isIndexedPng(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = file.read(26);
    static const QByteArray pngSignature("\x89PNG\r\n\x1a\n", 8);
    return header.size() == 26 && header.startsWith(pngSignature)
        && static_cast<unsigned char>(header.at(25)) == 3;
}

QString metadataValue(const QString &output, const QString &key)
{
    const QString prefix = key + QLatin1Char(' ');
    for (const QString &line : output.split(QLatin1Char('\n'))) {
        if (!line.startsWith(key))
            continue;
        QString value = line.mid(prefix.size()).trimmed();
        if (key == QStringLiteral("Exif.Photo.UserComment")
            && value.startsWith(QStringLiteral("charset=Unicode ")))
            value.remove(0, 16);
        return value;
    }
    return {};
}

bool writeMetadata(const QString &executable, const QString &path,
                   const ImageMetadata &metadata, QString &errorMessage)
{
    QStringList arguments;
    for (const MetadataField &field : metadataFields) {
        const QString key = QString::fromLatin1(field.key);
        const QString value = metadata.*(field.member);
        QString command;
        if (value.isEmpty()) {
            command = QStringLiteral("del %1").arg(key);
        } else if (key == QStringLiteral("Exif.Photo.UserComment")) {
            command = QStringLiteral("set %1 charset=Unicode %2").arg(key, value);
        } else {
            command = QStringLiteral("set %1 Ascii %2").arg(key, value);
        }
        arguments << QStringLiteral("-M") << command;
    }
    arguments << path;

    QProcess process;
    process.start(executable, arguments);
    if (!process.waitForStarted(5000)) {
        errorMessage = QStringLiteral("Exiv2 could not be started.");
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        errorMessage = QStringLiteral("Exiv2 timed out while saving image attributes.");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        errorMessage = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (errorMessage.isEmpty())
            errorMessage = QStringLiteral("Exiv2 could not save the image attributes.");
        return false;
    }
    return true;
}

} // namespace

bool ImageProcessor::isBackendAvailable()
{
    return !backendExecutable().isEmpty();
}

QString ImageProcessor::backendExecutable()
{
    const QString packagedExecutable = qEnvironmentVariable("PAPERCUTTER_IMAGEMAGICK");
    if (!packagedExecutable.isEmpty() && QFileInfo(packagedExecutable).isExecutable())
        return packagedExecutable;
    const QString modernExecutable = QStandardPaths::findExecutable(QStringLiteral("magick"));
    if (!modernExecutable.isEmpty())
        return modernExecutable;
    return QStandardPaths::findExecutable(QStringLiteral("convert"));
}

QString ImageProcessor::metadataExecutable()
{
    return QStandardPaths::findExecutable(QStringLiteral("exiv2"));
}

ImageMetadata ImageProcessor::readMetadata(const QString &path)
{
    ImageMetadata metadata;
    const QString executable = metadataExecutable();
    if (executable.isEmpty() || path.isEmpty())
        return metadata;

    QStringList arguments;
    for (const MetadataField &field : metadataFields)
        arguments << QStringLiteral("-K") << QString::fromLatin1(field.key);
    arguments << QStringLiteral("-Pkt") << path;

    QProcess process;
    process.start(executable, arguments);
    if (!process.waitForStarted(5000) || !process.waitForFinished(30000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return metadata;

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    for (const MetadataField &field : metadataFields) {
        const QString key = QString::fromLatin1(field.key);
        metadata.*(field.member) = metadataValue(output, key);
    }
    return metadata;
}

qint64 ImageProcessor::estimateOutputSize(const ImageJob &job, const QString &suffix)
{
    static const QSet<QString> supportedExtensions{
        QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("png"), QStringLiteral("webp")};
    const QString normalizedSuffix = suffix.toLower();
    if (!supportedExtensions.contains(normalizedSuffix))
        return -1;

    QTemporaryDir directory;
    if (!directory.isValid())
        return -1;
    const QString outputPath = directory.filePath(
        QStringLiteral("estimate.") + normalizedSuffix);
    const ProcessingResult result = ImageProcessor{}.process({job, outputPath, false});
    return result.succeeded ? QFileInfo(outputPath).size() : -1;
}

ProcessingResult ImageProcessor::process(const ProcessingRequest &request) const
{
    const ImageJob &job = request.job;
    ProcessingResult result;
    result.sourcePath = job.sourcePath;
    if (!job.composition.isValid())
        return {false, job.sourcePath, {},
                QStringLiteral("validation"), QStringLiteral("The composition is invalid.")};
    if (job.outputQuality < 1 || job.outputQuality > 100)
        return {false, job.sourcePath, {}, QStringLiteral("validation"),
                QStringLiteral("Output quality must be between 1 and 100.")};
    if (!QFileInfo::exists(job.sourcePath))
        return {false, job.sourcePath, {},
                QStringLiteral("validation"), QStringLiteral("The source image does not exist.")};
    if (!isBackendAvailable())
        return {false, job.sourcePath, {},
                QStringLiteral("backend"),
                QStringLiteral("ImageMagick is not installed; neither magick nor convert is in PATH.")};

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
                  << QStringLiteral("-quality") << QString::number(job.outputQuality);
    else if (outputSuffix == QStringLiteral("webp"))
        arguments << QStringLiteral("-quality") << QString::number(job.outputQuality);
    else if (outputSuffix == QStringLiteral("png")) {
        if (isIndexedPng(job.sourcePath)) {
            arguments << QStringLiteral("-colors") << QStringLiteral("256")
                      << QStringLiteral("-define") << QStringLiteral("png:color-type=3");
        }
        arguments << QStringLiteral("-define") << QStringLiteral("png:compression-level=9");
    }
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

    if (job.metadataEdited || !job.metadata.isEmpty()) {
        const QString executable = metadataExecutable();
        if (executable.isEmpty())
            return {false, job.sourcePath, outputPath, QStringLiteral("metadata"),
                    QStringLiteral("Exiv2 is required to save image attributes.")};
        QString metadataError;
        if (!writeMetadata(executable, temporaryPath, job.metadata, metadataError))
            return {false, job.sourcePath, outputPath, QStringLiteral("metadata"), metadataError};
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
