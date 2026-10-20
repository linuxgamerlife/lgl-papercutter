#pragma once

#include "app/settingsservice.h"
#include "core/imagejob.h"

#include <QMainWindow>
#include <QPoint>
#include <QSet>
#include <QVector>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;
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
    void populateResolutionTargets(bool selectPrimary = false);
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
    void showHelp();
    void showAbout();
    void setMenuBarVisible(bool visible);
    void loadCurrentJob(int row);
    void applyTargetResolutionToSelection();
    void updateCurrentZoomFromControl();
    void updateCurrentOutputQuality();
    void updateCurrentMetadataFromControls();
    void scheduleOutputSizeEstimate();
    void syncResolutionControlsToSelection();
    void updateQualityLabel();

    SettingsService m_settings;
    QVector<ImageJob> m_jobs;
    QListWidget *m_queue{nullptr};
    CompositionCanvas *m_canvas{nullptr};
    QComboBox *m_resolution{nullptr};
    QSpinBox *m_width{nullptr};
    QSpinBox *m_height{nullptr};
    QGroupBox *m_metadataGroup{nullptr};
    QLineEdit *m_description{nullptr};
    QLineEdit *m_artist{nullptr};
    QLineEdit *m_copyright{nullptr};
    QLineEdit *m_comment{nullptr};
    QSlider *m_zoom{nullptr};
    QLabel *m_outputQualityHeading{nullptr};
    QSlider *m_outputQuality{nullptr};
    QLabel *m_outputQualityValue{nullptr};
    QToolButton *m_resetQualityButton{nullptr};
    QLabel *m_fileSizeLabel{nullptr};
    QLabel *m_qualityLabel{nullptr};
    QLabel *m_sourceLabel{nullptr};
    QAction *m_toggleMenuAction{nullptr};
    QTimer *m_outputEstimateTimer{nullptr};
    quint64 m_outputEstimateGeneration{0};
    bool m_outputEstimateRunning{false};
    bool m_updatingControls{false};
};

} // namespace papercutter
