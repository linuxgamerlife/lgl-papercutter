#pragma once

#include "app/settingsservice.h"
#include "app/backupservice.h"
#include "core/imagejob.h"

#include <QMainWindow>
#include <QVector>

class QComboBox;
class QDockWidget;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QAction;
class QCheckBox;

namespace papercutter {

class CompositionCanvas;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void buildUi();
    void populateResolutionTargets();
    void addImages();
    void addFolder();
    void enqueuePaths(const QStringList &paths);
    void removeSelectedImages();
    void removeDuplicates();
    void chooseBackupFolder();
    void acceptCurrent();
    void acceptAllReviewed();
    void saveAsCurrent();
    void processAcceptedRows(const QVector<int> &rows);
    void setMenuBarVisible(bool visible);
    void reloadBackupHistory();
    void restoreSelectedBackup();
    void saveSelectedBackupAs();
    void openBackupFolder();
    void refreshProcessButton();
    void loadCurrentJob(int row);
    void updateCompositionFromControls();
    void updateQualityLabel();

    SettingsService m_settings;
    QVector<ImageJob> m_jobs;
    QListWidget *m_queue{nullptr};
    CompositionCanvas *m_canvas{nullptr};
    QComboBox *m_resolution{nullptr};
    QSpinBox *m_width{nullptr};
    QSpinBox *m_height{nullptr};
    QSlider *m_zoom{nullptr};
    QLabel *m_qualityLabel{nullptr};
    QLabel *m_sourceLabel{nullptr};
    QPushButton *m_processButton{nullptr};
    QAction *m_toggleMenuAction{nullptr};
    QDockWidget *m_backupDock{nullptr};
    QListWidget *m_backupList{nullptr};
    QCheckBox *m_filterBackups{nullptr};
    QVector<BackupRecord> m_backupRecords;
    bool m_updatingControls{false};
};

} // namespace papercutter
