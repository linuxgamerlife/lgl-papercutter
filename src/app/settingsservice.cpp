#include "app/settingsservice.h"

#include <QVariantList>

namespace papercutter {

SettingsService::SettingsService()
    : m_settings(QStringLiteral("LinuxGamerLife"), QStringLiteral("LGL Papercutter"))
{
}

QString SettingsService::lastSourceFolder() const
{
    return m_settings.value(QStringLiteral("files/lastSourceFolder")).toString();
}

void SettingsService::setLastSourceFolder(const QString &path)
{
    m_settings.setValue(QStringLiteral("files/lastSourceFolder"), path);
}

QString SettingsService::lastSaveAsFolder() const
{
    return m_settings.value(QStringLiteral("files/lastSaveAsFolder")).toString();
}

void SettingsService::setLastSaveAsFolder(const QString &path)
{
    m_settings.setValue(QStringLiteral("files/lastSaveAsFolder"), path);
}

bool SettingsService::menuBarVisible() const
{
    return m_settings.value(QStringLiteral("ui/menuBarVisible"), true).toBool();
}

void SettingsService::setMenuBarVisible(const bool visible)
{
    m_settings.setValue(QStringLiteral("ui/menuBarVisible"), visible);
}

QVector<QSize> SettingsService::resolutionPresets() const
{
    const QVariantList stored = m_settings.value(QStringLiteral("display/presets")).toList();
    QVector<QSize> presets;
    presets.reserve(stored.size());
    for (const QVariant &value : stored) {
        const QSize size = value.toSize();
        if (size.isValid())
            presets.push_back(size);
    }
    if (presets.isEmpty())
        presets = {
            {1920, 1080},
            {2560, 1080},
            {2560, 1440},
            {3440, 1440},
            {3840, 2160},
            {1080, 2560},
            {1440, 3440},
        };
    return presets;
}

void SettingsService::setResolutionPresets(const QVector<QSize> &presets)
{
    QVariantList stored;
    stored.reserve(presets.size());
    for (const QSize &size : presets) {
        if (size.isValid())
            stored.push_back(size);
    }
    m_settings.setValue(QStringLiteral("display/presets"), stored);
}

} // namespace papercutter
