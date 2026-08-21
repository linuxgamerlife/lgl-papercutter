#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QSize>
#include <QString>
#include <QVector>

namespace papercutter {

struct BackupRecord {
    QString id;
    QString sourcePath;
    QString backupPath;
    QDateTime timestamp;
    QString sha256;
    qint64 originalSize{0};
    int permissions{0};
    QSize processedSize;
    QString reason;
};

struct BackupResult {
    bool succeeded{false};
    BackupRecord record;
    QString errorMessage;
};

struct RestoreResult {
    bool succeeded{false};
    BackupRecord recoveryBackup;
    QString errorMessage;
};

class BackupService final {
public:
    explicit BackupService(QString backupFolder);

    BackupResult createBackup(const QString &sourcePath, const QSize &processedSize,
                              const QString &reason = QStringLiteral("replace"));
    RestoreResult restore(const BackupRecord &record);
    QVector<BackupRecord> records() const;

    static QString fileSha256(const QString &path, QString *errorMessage = nullptr);

private:
    QString manifestPath() const;
    bool appendRecord(const BackupRecord &record, QString *errorMessage);
    static BackupRecord fromJson(const QJsonObject &object);
    static QJsonObject toJson(const BackupRecord &record);

    QString m_backupFolder;
};

} // namespace papercutter
