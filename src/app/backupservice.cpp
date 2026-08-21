#include "app/backupservice.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QUuid>
#include <algorithm>
#include <filesystem>

namespace papercutter {

BackupService::BackupService(QString backupFolder)
    : m_backupFolder(std::move(backupFolder))
{
}

QString BackupService::fileSha256(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not read the complete file.");
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

BackupResult BackupService::createBackup(const QString &sourcePath,
                                         const QSize &processedSize,
                                         const QString &reason)
{
    const QFileInfo source(sourcePath);
    if (!source.isFile())
        return {false, {}, QStringLiteral("The source file does not exist.")};
    if (m_backupFolder.isEmpty())
        return {false, {}, QStringLiteral("A backup folder has not been configured.")};
    QDir folder(m_backupFolder);
    if (!folder.exists() && !folder.mkpath(QStringLiteral(".")))
        return {false, {}, QStringLiteral("The backup folder could not be created.")};

    BackupRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.sourcePath = source.absoluteFilePath();
    record.timestamp = QDateTime::currentDateTimeUtc();
    record.originalSize = source.size();
    record.permissions = static_cast<int>(source.permissions());
    record.processedSize = processedSize;
    record.reason = reason;
    const QString stamp = record.timestamp.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    record.backupPath = folder.filePath(QStringLiteral("%1-%2-%3.%4")
        .arg(source.completeBaseName(), stamp, record.id.left(8), source.suffix()));

    if (!QFile::copy(record.sourcePath, record.backupPath))
        return {false, {}, QStringLiteral("The original could not be copied to backup.")};
    QString hashError;
    record.sha256 = fileSha256(record.backupPath, &hashError);
    const QString sourceHash = fileSha256(record.sourcePath, &hashError);
    if (record.sha256.isEmpty() || record.sha256 != sourceHash
        || QFileInfo(record.backupPath).size() != record.originalSize) {
        QFile::remove(record.backupPath);
        return {false, {}, QStringLiteral("Backup verification failed: %1").arg(hashError)};
    }
    QString manifestError;
    if (!appendRecord(record, &manifestError)) {
        QFile::remove(record.backupPath);
        return {false, {}, QStringLiteral("Could not record backup history: %1")
                                   .arg(manifestError)};
    }
    return {true, record, {}};
}

RestoreResult BackupService::restore(const BackupRecord &record)
{
    QString hashError;
    if (fileSha256(record.backupPath, &hashError) != record.sha256)
        return {false, {}, QStringLiteral("The selected backup failed verification.")};
    if (!QFileInfo::exists(record.sourcePath))
        return {false, {}, QStringLiteral("The original source path no longer exists.")};

    const BackupResult recovery = createBackup(
        record.sourcePath, {}, QStringLiteral("pre-restore recovery"));
    if (!recovery.succeeded)
        return {false, {}, recovery.errorMessage};

    const QFileInfo source(record.sourcePath);
    QTemporaryFile temporary(source.absolutePath()
        + QStringLiteral("/.lgl-papercutter-restore-XXXXXX.") + source.suffix());
    if (!temporary.open())
        return {false, recovery.record, QStringLiteral("Could not create a restore file.")};
    const QString temporaryPath = temporary.fileName();
    temporary.close();
    QFile::remove(temporaryPath);
    if (!QFile::copy(record.backupPath, temporaryPath))
        return {false, recovery.record, QStringLiteral("Could not prepare the restore file.")};
    if (fileSha256(temporaryPath) != record.sha256) {
        QFile::remove(temporaryPath);
        return {false, recovery.record, QStringLiteral("Restore verification failed.")};
    }
    QFile::setPermissions(temporaryPath, static_cast<QFile::Permissions>(record.permissions));
    temporary.setAutoRemove(false);
    std::error_code error;
    std::filesystem::rename(temporaryPath.toStdString(), record.sourcePath.toStdString(), error);
    if (error) {
        QFile::remove(temporaryPath);
        return {false, recovery.record,
                QStringLiteral("Could not restore the source: %1")
                    .arg(QString::fromStdString(error.message()))};
    }
    return {true, recovery.record, {}};
}

QVector<BackupRecord> BackupService::records() const
{
    QFile file(manifestPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QVector<BackupRecord> result;
    for (const QJsonValue &value : document.array()) {
        if (value.isObject())
            result.push_back(fromJson(value.toObject()));
    }
    std::sort(result.begin(), result.end(), [](const BackupRecord &left,
                                               const BackupRecord &right) {
        return left.timestamp > right.timestamp;
    });
    return result;
}

QString BackupService::manifestPath() const
{
    return QDir(m_backupFolder).filePath(QStringLiteral(".lgl-papercutter-backups.json"));
}

bool BackupService::appendRecord(const BackupRecord &record, QString *errorMessage)
{
    QJsonArray array;
    QFile existing(manifestPath());
    if (existing.open(QIODevice::ReadOnly)) {
        const QJsonDocument current = QJsonDocument::fromJson(existing.readAll());
        if (current.isArray())
            array = current.array();
    }
    array.push_back(toJson(record));
    QSaveFile output(manifestPath());
    if (!output.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = output.errorString();
        return false;
    }
    output.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    if (!output.commit()) {
        if (errorMessage)
            *errorMessage = output.errorString();
        return false;
    }
    return true;
}

QJsonObject BackupService::toJson(const BackupRecord &record)
{
    return {{QStringLiteral("id"), record.id},
            {QStringLiteral("sourcePath"), record.sourcePath},
            {QStringLiteral("backupPath"), record.backupPath},
            {QStringLiteral("timestamp"), record.timestamp.toString(Qt::ISODateWithMs)},
            {QStringLiteral("sha256"), record.sha256},
            {QStringLiteral("originalSize"), record.originalSize},
            {QStringLiteral("permissions"), record.permissions},
            {QStringLiteral("processedWidth"), record.processedSize.width()},
            {QStringLiteral("processedHeight"), record.processedSize.height()},
            {QStringLiteral("reason"), record.reason}};
}

BackupRecord BackupService::fromJson(const QJsonObject &object)
{
    BackupRecord record;
    record.id = object.value(QStringLiteral("id")).toString();
    record.sourcePath = object.value(QStringLiteral("sourcePath")).toString();
    record.backupPath = object.value(QStringLiteral("backupPath")).toString();
    record.timestamp = QDateTime::fromString(
        object.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    record.sha256 = object.value(QStringLiteral("sha256")).toString();
    record.originalSize = object.value(QStringLiteral("originalSize")).toInteger();
    record.permissions = object.value(QStringLiteral("permissions")).toInt();
    record.processedSize = {
        object.value(QStringLiteral("processedWidth")).toInt(),
        object.value(QStringLiteral("processedHeight")).toInt()};
    record.reason = object.value(QStringLiteral("reason")).toString();
    return record;
}

} // namespace papercutter
