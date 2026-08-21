#include "ui/mainwindow.h"

#include "core/compositionstate.h"
#include "processing/imageprocessor.h"
#include "ui/compositioncanvas.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QIcon>
#include <QLabel>
#include <QKeySequence>
#include <QListWidget>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMap>
#include <QPushButton>
#include <QScreen>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <utility>

namespace papercutter {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    setAcceptDrops(true);
    populateResolutionTargets();
    setWindowTitle(QStringLiteral("LGL Papercutter"));
    resize(1280, 800);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::buildUi()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("Add &images…"), QKeySequence::Open,
                        this, &MainWindow::addImages);
    auto *removeAction = fileMenu->addAction(
        QStringLiteral("&Remove selected"), this, &MainWindow::removeSelectedImages);
    removeAction->setShortcut(QKeySequence(Qt::Key_Delete));
    auto *duplicatesAction = fileMenu->addAction(
        QStringLiteral("Remove &duplicates"), this, &MainWindow::removeDuplicates);
    fileMenu->addSeparator();
    auto *acceptAction = fileMenu->addAction(
        QStringLiteral("&Accept && Save"), this, &MainWindow::acceptCurrent);
    auto *acceptAllAction = fileMenu->addAction(
        QStringLiteral("Accept &all reviewed"), this, &MainWindow::acceptAllReviewed);
    auto *saveAsAction = fileMenu->addAction(
        QStringLiteral("Save &As…"), this, &MainWindow::saveAsCurrent);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence::Quit,
                        qApp, &QApplication::quit);

    auto *settingsMenu = menuBar()->addMenu(QStringLiteral("&Settings"));
    settingsMenu->addAction(QStringLiteral("Add wallpaper folder…"), this,
                            &MainWindow::addFolder);
    settingsMenu->addAction(QStringLiteral("Choose backup folder…"), this,
                            &MainWindow::chooseBackupFolder);
    m_toggleMenuAction = new QAction(QStringLiteral("Show Menu Bar"), this);
    m_toggleMenuAction->setCheckable(true);
    m_toggleMenuAction->setChecked(m_settings.menuBarVisible());
    m_toggleMenuAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    m_toggleMenuAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_toggleMenuAction, &QAction::toggled, this, &MainWindow::setMenuBarVisible);
    settingsMenu->addSeparator();
    settingsMenu->addAction(m_toggleMenuAction);
    addAction(m_toggleMenuAction);

    auto *toolbar = addToolBar(QStringLiteral("Files"));
    toolbar->setMovable(false);
    auto *addAction = toolbar->addAction(
        QIcon::fromTheme(QStringLiteral("list-add")), QStringLiteral("Add Images"),
        this, &MainWindow::addImages);
    addAction->setToolTip(QStringLiteral("Add images to the queue"));
    removeAction->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    toolbar->addAction(removeAction);
    duplicatesAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    toolbar->addAction(duplicatesAction);
    toolbar->addSeparator();
    acceptAction->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    toolbar->addAction(acceptAction);

    auto *moreButton = new QToolButton(toolbar);
    moreButton->setText(QStringLiteral("More"));
    moreButton->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    moreButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(moreButton);
    moreMenu->addAction(saveAsAction);
    moreMenu->addAction(acceptAllAction);
    moreMenu->addSeparator();
    moreMenu->addAction(m_toggleMenuAction);
    moreButton->setMenu(moreMenu);
    toolbar->addWidget(moreButton);

    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    auto *splitter = new QSplitter(Qt::Horizontal, central);

    m_queue = new QListWidget(splitter);
    m_queue->setMinimumWidth(220);
    m_queue->setAccessibleName(QStringLiteral("Processing queue"));
    m_queue->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_canvas = new CompositionCanvas(splitter);

    auto *controls = new QWidget(splitter);
    controls->setMinimumWidth(280);
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->addWidget(new QLabel(QStringLiteral("<b>Target resolution</b>"), controls));
    m_resolution = new QComboBox(controls);
    controlsLayout->addWidget(m_resolution);

    auto *dimensions = new QHBoxLayout;
    m_width = new QSpinBox(controls);
    m_width->setRange(1, 32768);
    m_width->setValue(3440);
    m_height = new QSpinBox(controls);
    m_height->setRange(1, 32768);
    m_height->setValue(1440);
    dimensions->addWidget(m_width);
    dimensions->addWidget(new QLabel(QStringLiteral("×"), controls));
    dimensions->addWidget(m_height);
    controlsLayout->addLayout(dimensions);

    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(new QLabel(QStringLiteral("<b>Zoom</b>"), controls));
    m_zoom = new QSlider(Qt::Horizontal, controls);
    m_zoom->setRange(5, 2000);
    m_zoom->setValue(100);
    controlsLayout->addWidget(m_zoom);

    m_qualityLabel = new QLabel(QStringLiteral("Add an image to see scale information."), controls);
    m_qualityLabel->setWordWrap(true);
    controlsLayout->addWidget(m_qualityLabel);
    controlsLayout->addStretch();

    m_sourceLabel = new QLabel(QStringLiteral("Source: no image selected"), controls);
    m_sourceLabel->setWordWrap(true);
    controlsLayout->addWidget(m_sourceLabel);

    m_processButton = new QPushButton(QStringLiteral("Accept && Save"), controls);
    m_processButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    controlsLayout->addWidget(m_processButton);

    splitter->addWidget(m_queue);
    splitter->addWidget(m_canvas);
    splitter->addWidget(controls);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter);
    setCentralWidget(central);

    connect(m_queue, &QListWidget::currentRowChanged, this, &MainWindow::loadCurrentJob);
    connect(m_processButton, &QPushButton::clicked, this, &MainWindow::acceptCurrent);
    connect(m_width, &QSpinBox::valueChanged, this, &MainWindow::updateCompositionFromControls);
    connect(m_height, &QSpinBox::valueChanged, this, &MainWindow::updateCompositionFromControls);
    connect(m_zoom, &QSlider::valueChanged, this, &MainWindow::updateCompositionFromControls);
    connect(m_resolution, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_updatingControls || index < 0)
            return;
        const QSize size = m_resolution->itemData(index).toSize();
        if (!size.isValid())
            return;
        m_updatingControls = true;
        m_width->setValue(size.width());
        m_height->setValue(size.height());
        m_updatingControls = false;
        updateCompositionFromControls();
    });
    connect(m_canvas, &CompositionCanvas::compositionChanged, this,
            [this](const CompositionState &state) {
        const int row = m_queue->currentRow();
        if (row < 0 || row >= m_jobs.size())
            return;
        m_jobs[row].composition = state;
        m_updatingControls = true;
        m_zoom->setValue(qRound(state.zoom * 100.0));
        m_updatingControls = false;
        updateQualityLabel();
    });

    m_backupDock = new QDockWidget(QStringLiteral("Backup History"), this);
    m_backupDock->setObjectName(QStringLiteral("backupHistoryDock"));
    auto *historyWidget = new QWidget(m_backupDock);
    auto *historyLayout = new QVBoxLayout(historyWidget);
    m_filterBackups = new QCheckBox(QStringLiteral("Selected image only"), historyWidget);
    historyLayout->addWidget(m_filterBackups);
    m_backupList = new QListWidget(historyWidget);
    m_backupList->setAccessibleName(QStringLiteral("Backup history"));
    historyLayout->addWidget(m_backupList);
    auto *historyButtons = new QHBoxLayout;
    auto *restoreButton = new QPushButton(QStringLiteral("Restore"), historyWidget);
    auto *saveBackupButton = new QPushButton(QStringLiteral("Save Backup As…"), historyWidget);
    auto *openBackupButton = new QPushButton(QStringLiteral("Open Folder"), historyWidget);
    historyButtons->addWidget(restoreButton);
    historyButtons->addWidget(saveBackupButton);
    historyButtons->addWidget(openBackupButton);
    historyLayout->addLayout(historyButtons);
    m_backupDock->setWidget(historyWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_backupDock);
    m_backupDock->hide();
    settingsMenu->addAction(m_backupDock->toggleViewAction());
    moreMenu->addAction(m_backupDock->toggleViewAction());
    connect(restoreButton, &QPushButton::clicked, this, &MainWindow::restoreSelectedBackup);
    connect(saveBackupButton, &QPushButton::clicked, this, &MainWindow::saveSelectedBackupAs);
    connect(openBackupButton, &QPushButton::clicked, this, &MainWindow::openBackupFolder);
    connect(m_filterBackups, &QCheckBox::toggled, this, &MainWindow::reloadBackupHistory);

    setMenuBarVisible(m_settings.menuBarVisible());
    reloadBackupHistory();
    refreshProcessButton();
}

void MainWindow::populateResolutionTargets()
{
    m_resolution->clear();
    for (QScreen *screen : QGuiApplication::screens()) {
        const QSize pixels = screen->size() * screen->devicePixelRatio();
        m_resolution->addItem(QStringLiteral("%1 — %2 × %3")
                                  .arg(screen->name())
                                  .arg(pixels.width())
                                  .arg(pixels.height()), pixels);
    }
    for (const QSize &size : m_settings.resolutionPresets()) {
        m_resolution->addItem(QStringLiteral("Preset — %1 × %2")
                                  .arg(size.width()).arg(size.height()), size);
    }
}

void MainWindow::addImages()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add wallpapers"), m_settings.lastSourceFolder(),
        QStringLiteral("Images (*.jpg *.jpeg *.png *.webp)"));
    enqueuePaths(paths);
}

void MainWindow::addFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Add wallpaper folder"), m_settings.lastSourceFolder());
    if (folder.isEmpty())
        return;
    enqueuePaths({folder});
}

void MainWindow::enqueuePaths(const QStringList &paths)
{
    if (paths.isEmpty())
        return;

    static const QSet<QString> supportedExtensions{
        QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("png"), QStringLiteral("webp")};
    QSet<QString> queuedPaths;
    for (const ImageJob &job : std::as_const(m_jobs))
        queuedPaths.insert(QFileInfo(job.sourcePath).absoluteFilePath());

    QStringList candidateFiles;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isDir()) {
            const QFileInfoList files = QDir(info.absoluteFilePath()).entryInfoList(
                QDir::Files | QDir::Readable, QDir::Name);
            for (const QFileInfo &file : files) {
                if (supportedExtensions.contains(file.suffix().toLower()))
                    candidateFiles.push_back(file.absoluteFilePath());
            }
            m_settings.setLastSourceFolder(info.absoluteFilePath());
        } else if (info.isFile()
                   && supportedExtensions.contains(info.suffix().toLower())) {
            candidateFiles.push_back(info.absoluteFilePath());
            m_settings.setLastSourceFolder(info.absolutePath());
        }
    }

    int added = 0;
    for (const QString &path : std::as_const(candidateFiles)) {
        if (queuedPaths.contains(path))
            continue;
        QImageReader reader(path);
        const QSize size = reader.size();
        if (!size.isValid())
            continue;
        ImageJob job;
        job.sourcePath = path;
        job.sourceSize = size;
        job.composition.targetSize = {m_width->value(), m_height->value()};
        m_jobs.push_back(job);
        m_queue->addItem(job.displayName());
        queuedPaths.insert(path);
        ++added;
    }
    if (m_queue->currentRow() < 0 && !m_jobs.isEmpty())
        m_queue->setCurrentRow(0);
    statusBar()->showMessage(QStringLiteral("Added %1 image(s) to the queue.").arg(added), 5000);
    refreshProcessButton();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths.push_back(url.toLocalFile());
    }
    enqueuePaths(paths);
    event->acceptProposedAction();
}

void MainWindow::removeSelectedImages()
{
    const QModelIndexList selected = m_queue->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    QSet<int> selectedRows;
    int nextRow = static_cast<int>(m_jobs.size());
    for (const QModelIndex &index : selected) {
        selectedRows.insert(index.row());
        nextRow = std::min(nextRow, index.row());
    }

    QVector<ImageJob> remainingJobs;
    remainingJobs.reserve(m_jobs.size() - selectedRows.size());
    for (int row = 0; row < m_jobs.size(); ++row) {
        if (!selectedRows.contains(row))
            remainingJobs.push_back(m_jobs[row]);
    }
    m_jobs = std::move(remainingJobs);

    const QSignalBlocker blocker(m_queue);
    m_queue->clear();
    for (const ImageJob &job : std::as_const(m_jobs))
        m_queue->addItem(job.displayName());
    if (!m_jobs.isEmpty())
        m_queue->setCurrentRow(
            std::min(nextRow, static_cast<int>(m_jobs.size()) - 1));

    if (m_jobs.isEmpty()) {
        m_canvas->setImage({});
        updateQualityLabel();
    } else {
        loadCurrentJob(m_queue->currentRow());
    }
    refreshProcessButton();
}

void MainWindow::chooseBackupFolder()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose backup folder"), m_settings.backupFolder());
    if (path.isEmpty())
        return;
    m_settings.setBackupFolder(path);
    reloadBackupHistory();
    refreshProcessButton();
}

void MainWindow::refreshProcessButton()
{
    const bool ready = m_queue->currentRow() >= 0
        && !m_settings.backupFolder().isEmpty()
        && ImageProcessor::isBackendAvailable();
    m_processButton->setEnabled(ready);
    if (!ImageProcessor::isBackendAvailable())
        m_processButton->setToolTip(QStringLiteral("Install ImageMagick to process images."));
    else if (m_settings.backupFolder().isEmpty())
        m_processButton->setToolTip(QStringLiteral("Choose a backup folder first."));
    else if (m_queue->currentRow() < 0)
        m_processButton->setToolTip(QStringLiteral("Select an image first."));
    else
        m_processButton->setToolTip(
            QStringLiteral("Back up, accept, and replace the selected image."));
}

void MainWindow::acceptCurrent()
{
    const int row = m_queue->currentRow();
    if (row < 0 || !m_processButton->isEnabled())
        return;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Accept edit"),
        QStringLiteral("Back up and replace %1 with this edit?\n\n"
                       "The filename and location will be retained.")
            .arg(m_jobs[row].displayName()));
    if (answer != QMessageBox::Yes)
        return;
    processAcceptedRows({row});
}

void MainWindow::acceptAllReviewed()
{
    if (m_jobs.isEmpty() || m_settings.backupFolder().isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Accept all edits"),
        QStringLiteral("Back up and replace all %1 queued images?").arg(m_jobs.size()));
    if (answer != QMessageBox::Yes)
        return;
    QVector<int> rows;
    rows.reserve(m_jobs.size());
    for (int row = 0; row < m_jobs.size(); ++row)
        rows.push_back(row);
    processAcceptedRows(rows);
}

void MainWindow::processAcceptedRows(const QVector<int> &rows)
{
    if (rows.isEmpty())
        return;

    m_processButton->setEnabled(false);
    statusBar()->showMessage(QStringLiteral("Accepting edits…"));
    QVector<ProcessingRequest> requests;
    requests.reserve(rows.size());
    const QString backupFolder = m_settings.backupFolder();
    for (const int row : rows) {
        if (row >= 0 && row < m_jobs.size())
            requests.push_back({m_jobs[row], ProcessingOperation::AcceptAndReplace,
                                {}, backupFolder, false});
    }
    auto *watcher = new QFutureWatcher<QVector<ProcessingResult>>(this);
    connect(watcher, &QFutureWatcher<QVector<ProcessingResult>>::finished, this,
            [this, watcher, rows] {
        const QVector<ProcessingResult> results = watcher->result();
        watcher->deleteLater();
        int succeeded = 0;
        QStringList failures;
        QSet<int> successfulRows;
        QMap<int, QString> failedRows;
        for (int index = 0; index < results.size(); ++index) {
            const int originalRow = rows.value(index, -1);
            if (results[index].succeeded) {
                ++succeeded;
                successfulRows.insert(originalRow);
            } else {
                failedRows.insert(originalRow, results[index].errorMessage);
                failures << QStringLiteral("%1: %2")
                                .arg(QFileInfo(results[index].sourcePath).fileName(),
                                     results[index].errorMessage);
            }
        }
        QVector<ImageJob> remainingJobs;
        for (int row = 0; row < m_jobs.size(); ++row) {
            if (successfulRows.contains(row))
                continue;
            ImageJob job = m_jobs[row];
            if (failedRows.contains(row)) {
                job.status = JobStatus::Failed;
                job.errorMessage = failedRows.value(row);
            }
            remainingJobs.push_back(job);
        }
        m_jobs = std::move(remainingJobs);
        m_queue->clear();
        for (const ImageJob &job : std::as_const(m_jobs)) {
            m_queue->addItem(job.status == JobStatus::Failed
                ? QStringLiteral("⚠ %1").arg(job.displayName()) : job.displayName());
        }
        if (!m_jobs.isEmpty())
            m_queue->setCurrentRow(0);
        else
            m_canvas->setImage({});
        statusBar()->showMessage(QStringLiteral("Accepted %1 of %2 image(s).")
                                     .arg(succeeded).arg(results.size()), 10000);
        if (!failures.isEmpty())
            QMessageBox::warning(this, QStringLiteral("Some images failed"), failures.join('\n'));
        else
            QMessageBox::information(this, QStringLiteral("Edits accepted"),
                                     QStringLiteral("All originals were backed up and replaced."));
        if (!m_jobs.isEmpty())
            loadCurrentJob(m_queue->currentRow());
        reloadBackupHistory();
        refreshProcessButton();
    });
    watcher->setFuture(QtConcurrent::run([requests] {
        QVector<ProcessingResult> results;
        results.reserve(requests.size());
        const ImageProcessor processor;
        for (const ProcessingRequest &request : requests)
            results.push_back(processor.process(request));
        return results;
    }));
}

void MainWindow::saveAsCurrent()
{
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;
    const QFileInfo source(m_jobs[row].sourcePath);
    const QString initialFolder = m_settings.lastSaveAsFolder().isEmpty()
        ? source.absolutePath() : m_settings.lastSaveAsFolder();
    const QString destination = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save processed image as"),
        QDir(initialFolder).filePath(source.fileName()),
        QStringLiteral("Images (*.jpg *.jpeg *.png *.webp)"), nullptr,
        QFileDialog::DontConfirmOverwrite);
    if (destination.isEmpty())
        return;
    if (QFileInfo(destination).absoluteFilePath() == source.absoluteFilePath()) {
        QMessageBox::information(this, QStringLiteral("Use Accept & Save"),
            QStringLiteral("Use Accept & Save to safely back up and replace the original."));
        return;
    }
    bool overwrite = false;
    if (QFileInfo::exists(destination)) {
        overwrite = QMessageBox::question(
            this, QStringLiteral("Replace existing file?"),
            QStringLiteral("%1 already exists. Replace it?").arg(destination))
            == QMessageBox::Yes;
        if (!overwrite)
            return;
    }
    statusBar()->showMessage(QStringLiteral("Saving copy…"));
    const ProcessingRequest request{m_jobs[row], ProcessingOperation::SaveAs,
                                    destination, {}, overwrite};
    auto *watcher = new QFutureWatcher<ProcessingResult>(this);
    connect(watcher, &QFutureWatcher<ProcessingResult>::finished, this,
            [this, watcher, destination] {
        const ProcessingResult result = watcher->result();
        watcher->deleteLater();
        if (!result.succeeded) {
            QMessageBox::warning(this, QStringLiteral("Save As failed"), result.errorMessage);
            statusBar()->showMessage(QStringLiteral("Save As failed."), 5000);
            return;
        }
        m_settings.setLastSaveAsFolder(QFileInfo(destination).absolutePath());
        statusBar()->showMessage(QStringLiteral("Saved %1").arg(destination), 10000);
    });
    watcher->setFuture(QtConcurrent::run([request] {
        return ImageProcessor{}.process(request);
    }));
}

void MainWindow::setMenuBarVisible(const bool visible)
{
    menuBar()->setVisible(visible);
    if (m_toggleMenuAction->isChecked() != visible)
        m_toggleMenuAction->setChecked(visible);
    m_settings.setMenuBarVisible(visible);
}

void MainWindow::reloadBackupHistory()
{
    const QVector<BackupRecord> allRecords =
        BackupService(m_settings.backupFolder()).records();
    m_backupRecords.clear();
    const int currentRow = m_queue->currentRow();
    const QString selectedSource = currentRow >= 0 && currentRow < m_jobs.size()
        ? QFileInfo(m_jobs[currentRow].sourcePath).absoluteFilePath() : QString();
    for (const BackupRecord &record : allRecords) {
        if (!m_filterBackups->isChecked()
            || QFileInfo(record.sourcePath).absoluteFilePath() == selectedSource)
            m_backupRecords.push_back(record);
    }
    m_backupList->clear();
    for (const BackupRecord &record : std::as_const(m_backupRecords)) {
        m_backupList->addItem(QStringLiteral("%1 — %2 — %3")
            .arg(QFileInfo(record.sourcePath).fileName(),
                 record.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 record.reason));
    }
}

void MainWindow::saveSelectedBackupAs()
{
    const int row = m_backupList->currentRow();
    if (row < 0 || row >= m_backupRecords.size())
        return;
    const BackupRecord &record = m_backupRecords[row];
    const QFileInfo backup(record.backupPath);
    const QString initialFolder = m_settings.lastSaveAsFolder().isEmpty()
        ? QFileInfo(record.sourcePath).absolutePath() : m_settings.lastSaveAsFolder();
    const QString destination = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save backup as"),
        QDir(initialFolder).filePath(QFileInfo(record.sourcePath).fileName()),
        QStringLiteral("Images (*.jpg *.jpeg *.png *.webp)"), nullptr,
        QFileDialog::DontConfirmOverwrite);
    if (destination.isEmpty())
        return;
    if (QFileInfo::exists(destination)
        && QMessageBox::question(this, QStringLiteral("Replace existing file?"),
               QStringLiteral("%1 already exists. Replace it?").arg(destination))
            != QMessageBox::Yes)
        return;
    QDir().mkpath(QFileInfo(destination).absolutePath());
    QFile input(backup.absoluteFilePath());
    QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)
        || output.write(input.readAll()) < 0 || !output.commit()) {
        QMessageBox::warning(this, QStringLiteral("Save backup failed"),
                             QStringLiteral("The backup could not be copied."));
        return;
    }
    m_settings.setLastSaveAsFolder(QFileInfo(destination).absolutePath());
    statusBar()->showMessage(QStringLiteral("Saved backup copy to %1").arg(destination), 10000);
}

void MainWindow::restoreSelectedBackup()
{
    const int row = m_backupList->currentRow();
    if (row < 0 || row >= m_backupRecords.size())
        return;
    const BackupRecord record = m_backupRecords[row];
    if (QMessageBox::question(this, QStringLiteral("Restore backup"),
        QStringLiteral("Restore %1?\n\nThe current file will receive a recovery backup first.")
            .arg(record.sourcePath)) != QMessageBox::Yes)
        return;
    auto *watcher = new QFutureWatcher<RestoreResult>(this);
    connect(watcher, &QFutureWatcher<RestoreResult>::finished, this, [this, watcher] {
        const RestoreResult result = watcher->result();
        watcher->deleteLater();
        if (result.succeeded)
            QMessageBox::information(this, QStringLiteral("Restore complete"),
                                     QStringLiteral("The backup was restored safely."));
        else
            QMessageBox::warning(this, QStringLiteral("Restore failed"), result.errorMessage);
        reloadBackupHistory();
        if (m_queue->currentRow() >= 0)
            loadCurrentJob(m_queue->currentRow());
    });
    const QString folder = m_settings.backupFolder();
    watcher->setFuture(QtConcurrent::run([folder, record] {
        return BackupService(folder).restore(record);
    }));
}

void MainWindow::openBackupFolder()
{
    const QString folder = m_settings.backupFolder();
    if (!folder.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MainWindow::removeDuplicates()
{
    if (m_jobs.size() < 2)
        return;
    statusBar()->showMessage(QStringLiteral("Checking for duplicate content…"));
    const QVector<ImageJob> jobs = m_jobs;
    auto *watcher = new QFutureWatcher<QVector<int>>(this);
    connect(watcher, &QFutureWatcher<QVector<int>>::finished, this, [this, watcher] {
        const QVector<int> rows = watcher->result();
        watcher->deleteLater();
        m_queue->clearSelection();
        for (const int row : rows) {
            if (auto *item = m_queue->item(row))
                item->setSelected(true);
        }
        if (!rows.isEmpty())
            removeSelectedImages();
        statusBar()->showMessage(QStringLiteral("Removed %1 duplicate queue item(s).")
                                     .arg(rows.size()), 5000);
    });
    watcher->setFuture(QtConcurrent::run([jobs] {
        QSet<QString> hashes;
        QVector<int> duplicates;
        for (int row = 0; row < jobs.size(); ++row) {
            const QString hash = BackupService::fileSha256(jobs[row].sourcePath);
            if (hash.isEmpty())
                continue;
            if (hashes.contains(hash))
                duplicates.push_back(row);
            else
                hashes.insert(hash);
        }
        return duplicates;
    }));
}

void MainWindow::loadCurrentJob(const int row)
{
    if (row < 0 || row >= m_jobs.size()) {
        m_canvas->setImage({});
        m_sourceLabel->setText(QStringLiteral("Source: no image selected"));
        refreshProcessButton();
        return;
    }

    ImageJob &job = m_jobs[row];
    QImageReader reader(job.sourcePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Unable to load image"), reader.errorString());
        return;
    }
    job.sourceSize = image.size();
    m_sourceLabel->setText(QStringLiteral("Source: %1").arg(job.sourcePath));
    m_canvas->setImage(image);
    m_canvas->setComposition(job.composition);

    m_updatingControls = true;
    m_width->setValue(job.composition.targetSize.width());
    m_height->setValue(job.composition.targetSize.height());
    m_zoom->setValue(qRound(job.composition.zoom * 100.0));
    m_updatingControls = false;
    updateQualityLabel();
    refreshProcessButton();
    if (m_filterBackups && m_filterBackups->isChecked())
        reloadBackupHistory();
}

void MainWindow::updateCompositionFromControls()
{
    if (m_updatingControls)
        return;
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;

    ImageJob &job = m_jobs[row];
    job.composition.targetSize = {m_width->value(), m_height->value()};
    job.composition.zoom = m_zoom->value() / 100.0;
    m_canvas->setComposition(job.composition);
    updateQualityLabel();
}

void MainWindow::updateQualityLabel()
{
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size()) {
        m_qualityLabel->setText(QStringLiteral("Add an image to see scale information."));
        return;
    }

    const ImageJob &job = m_jobs[row];
    const double scale = job.composition.effectiveScaleFor(job.sourceSize);
    const QString warning = scale > 1.0
        ? QStringLiteral(" — enlargement may look soft") : QString();
    m_qualityLabel->setText(QStringLiteral("Effective scale: %1×%2")
                                .arg(scale, 0, 'f', 2).arg(warning));
}

} // namespace papercutter
