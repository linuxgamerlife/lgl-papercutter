#pragma once

#include <QSettings>
#include <QSize>
#include <QString>
#include <QVector>

namespace papercutter {

class SettingsService final {
public:
    SettingsService();

    QString backupFolder() const;
    void setBackupFolder(const QString &path);

    QString lastSourceFolder() const;
    void setLastSourceFolder(const QString &path);

    QString lastSaveAsFolder() const;
    void setLastSaveAsFolder(const QString &path);

    bool menuBarVisible() const;
    void setMenuBarVisible(bool visible);

    QVector<QSize> resolutionPresets() const;
    void setResolutionPresets(const QVector<QSize> &presets);

private:
    QSettings m_settings;
};

} // namespace papercutter
