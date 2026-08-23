#pragma once

#include "app/settingsservice.h"
#include "core/imagejob.h"

#include <QMainWindow>
#include <QPoint>
#include <QSet>
#include <QVector>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QAction;

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
    QVector<int> selectedJobRows() const;
    void removeJobRows(const QSet<int> &rows);
    void showQueueContextMenu(const QPoint &position);
    void removeSelectedImages();
    void moveSelectedImagesToTrash();
    void removeDuplicates();
    void saveAsCurrent();
    void saveSelectedAs();
    void setMenuBarVisible(bool visible);
    void loadCurrentJob(int row);
    void applyTargetResolutionToSelection();
    void updateCurrentZoomFromControl();
    void syncResolutionControlsToSelection();
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
    QAction *m_toggleMenuAction{nullptr};
    bool m_updatingControls{false};
};

} // namespace papercutter
