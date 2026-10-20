#include "ui/mainwindow.h"

#include "core/compositionstate.h"
#include "processing/imageprocessor.h"
#include "ui/compositioncanvas.h"

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QIcon>
#include <QLabel>
#include <QKeySequence>
#include <QListWidget>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <limits>
#include <utility>

namespace papercutter {

namespace {

QString formatFileSize(const qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("Unavailable");
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KiB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MiB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    return QStringLiteral("%1 GiB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

QString outputFormatName(const QString &suffix)
{
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg"))
        return QStringLiteral("JPEG");
    if (suffix == QStringLiteral("webp"))
        return QStringLiteral("WebP");
    if (suffix == QStringLiteral("png"))
        return QStringLiteral("PNG");
    return suffix.toUpper();
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) ? QString::fromLatin1(hash.result().toHex()) : QString();
}

struct DestinationNumbering {
    bool foundNumberedFile{false};
    qulonglong lowestNumber{0};
    qulonglong highestNumber{0};
    qulonglong nextCandidate{0};
    qsizetype paddingWidth{0};
    QSet<qulonglong> usedNumbers;
};

DestinationNumbering inspectDestinationNumbering(const QString &folder)
{
    DestinationNumbering numbering;
    const QRegularExpression numberedStem(QStringLiteral("^\\d+$"));
    const QFileInfoList files = QDir(folder).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files) {
        const QString stem = file.completeBaseName();
        if (!numberedStem.match(stem).hasMatch())
            continue;
        bool converted = false;
        const qulonglong number = stem.toULongLong(&converted);
        if (!converted)
            continue;
        if (!numbering.foundNumberedFile) {
            numbering.foundNumberedFile = true;
            numbering.lowestNumber = number;
            numbering.highestNumber = number;
        } else {
            numbering.lowestNumber = std::min(numbering.lowestNumber, number);
            numbering.highestNumber = std::max(numbering.highestNumber, number);
        }
        numbering.paddingWidth = std::max(numbering.paddingWidth, stem.size());
        numbering.usedNumbers.insert(number);
    }
    numbering.nextCandidate = numbering.lowestNumber;
    return numbering;
}

bool takeNextAvailableNumber(DestinationNumbering &numbering, qulonglong &number)
{
    if (!numbering.foundNumberedFile)
        return false;
    while (numbering.usedNumbers.contains(numbering.nextCandidate)) {
        if (numbering.nextCandidate == std::numeric_limits<qulonglong>::max())
            return false;
        ++numbering.nextCandidate;
    }
    number = numbering.nextCandidate;
    numbering.usedNumbers.insert(number);
    if (numbering.nextCandidate != std::numeric_limits<qulonglong>::max())
        ++numbering.nextCandidate;
    return true;
}

bool confirmExportMappings(QWidget *parent, const QVector<ProcessingRequest> &requests)
{
    QDialog confirmation(parent);
    confirmation.setWindowTitle(QStringLiteral("Confirm Save As"));
    confirmation.setModal(true);
    confirmation.resize(820, 360);

    auto *layout = new QVBoxLayout(&confirmation);
    auto *summary = new QLabel(requests.size() == 1
        ? QStringLiteral("Review the file that will be created:")
        : QStringLiteral("Review the %1 files that will be created:").arg(requests.size()),
        &confirmation);
    layout->addWidget(summary);

    auto *files = new QTreeWidget(&confirmation);
    files->setColumnCount(2);
    files->setHeaderLabels({QStringLiteral("Source"), QStringLiteral("Destination")});
    files->setRootIsDecorated(false);
    files->setAlternatingRowColors(true);
    files->setSelectionMode(QAbstractItemView::NoSelection);
    files->setTextElideMode(Qt::ElideMiddle);
    files->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    files->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    files->setColumnWidth(0, 300);
    for (const ProcessingRequest &request : requests) {
        auto *item = new QTreeWidgetItem(files);
        item->setText(0, QFileInfo(request.job.sourcePath).fileName());
        item->setText(1, QDir::toNativeSeparators(request.destinationPath));
        item->setToolTip(0, QDir::toNativeSeparators(request.job.sourcePath));
        item->setToolTip(1, QDir::toNativeSeparators(request.destinationPath));
    }
    layout->addWidget(files, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         &confirmation);
    buttons->button(QDialogButtonBox::Save)->setText(
        requests.size() == 1 ? QStringLiteral("Save File") : QStringLiteral("Save Files"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &confirmation, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &confirmation, &QDialog::reject);
    layout->addWidget(buttons);

    return confirmation.exec() == QDialog::Accepted;
}

bool chooseSequentialNumbering(QWidget *parent, const DestinationNumbering &numbering,
                               bool &useSequentialNumbering)
{
    useSequentialNumbering = false;
    if (!numbering.foundNumberedFile)
        return true;

    DestinationNumbering preview = numbering;
    qulonglong firstAvailableNumber = 0;
    if (!takeNextAvailableNumber(preview, firstAvailableNumber))
        return true;

    const QString highest = QString::number(numbering.highestNumber).rightJustified(
        numbering.paddingWidth, QLatin1Char('0'));
    const QString firstOutput = QString::number(firstAvailableNumber).rightJustified(
        numbering.paddingWidth, QLatin1Char('0'));

    QMessageBox options(parent);
    options.setIcon(QMessageBox::Question);
    options.setWindowTitle(QStringLiteral("File numbering"));
    options.setText(firstAvailableNumber <= numbering.highestNumber
        ? QStringLiteral("The highest numbered file is %1, and the first gap is %2.")
              .arg(highest, firstOutput)
        : QStringLiteral("The highest numbered file in this folder is %1.").arg(highest));
    auto *renumber = new QCheckBox(
        QStringLiteral("Number exported files sequentially (start at %1)").arg(firstOutput),
        &options);
    renumber->setChecked(true);
    options.setCheckBox(renumber);
    options.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    options.setDefaultButton(QMessageBox::Ok);
    options.button(QMessageBox::Ok)->setText(QStringLiteral("Preview Files"));
    if (options.exec() != QMessageBox::Ok)
        return false;
    useSequentialNumbering = renumber->isChecked();
    return true;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    setAcceptDrops(true);
    populateResolutionTargets(true);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this,
            [this](QScreen *) { populateResolutionTargets(m_jobs.isEmpty()); });
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
    auto *selectAllAction = fileMenu->addAction(QStringLiteral("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this] {
        if (m_queue)
            m_queue->selectAll();
    });
    auto *duplicatesAction = fileMenu->addAction(
        QStringLiteral("Remove &duplicates"), this, &MainWindow::removeDuplicates);
    fileMenu->addSeparator();
    auto *saveAsAction = fileMenu->addAction(
        QStringLiteral("Save &As…"), this, &MainWindow::saveSelectedAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence::Quit,
                        qApp, &QApplication::quit);

    auto *settingsMenu = menuBar()->addMenu(QStringLiteral("&Settings"));
    settingsMenu->addAction(QStringLiteral("Add wallpaper folder…"), this,
                            &MainWindow::addFolder);
    m_toggleMenuAction = new QAction(QStringLiteral("Show Menu Bar"), this);
    m_toggleMenuAction->setCheckable(true);
    m_toggleMenuAction->setChecked(m_settings.menuBarVisible());
    m_toggleMenuAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    m_toggleMenuAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_toggleMenuAction, &QAction::toggled, this, &MainWindow::setMenuBarVisible);
    settingsMenu->addSeparator();
    settingsMenu->addAction(m_toggleMenuAction);
    addAction(m_toggleMenuAction);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    auto *helpAction = helpMenu->addAction(QStringLiteral("LGL Papercutter &Help"),
                                           this, &MainWindow::showHelp);
    helpAction->setShortcut(QKeySequence::HelpContents);
    helpMenu->addSeparator();
    auto *aboutAction = helpMenu->addAction(QStringLiteral("&About LGL Papercutter"),
                                             this, &MainWindow::showAbout);

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

    auto *moreButton = new QToolButton(toolbar);
    moreButton->setText(QStringLiteral("More"));
    moreButton->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    moreButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(moreButton);
    moreMenu->addAction(saveAsAction);
    moreMenu->addSeparator();
    moreMenu->addAction(helpAction);
    moreMenu->addAction(aboutAction);
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
    m_queue->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_queue, &QListWidget::customContextMenuRequested,
            this, &MainWindow::showQueueContextMenu);

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

    m_metadataGroup = new QGroupBox(QStringLiteral("Image attributes"), controls);
    auto *metadataLayout = new QFormLayout(m_metadataGroup);
    m_description = new QLineEdit(m_metadataGroup);
    m_artist = new QLineEdit(m_metadataGroup);
    m_copyright = new QLineEdit(m_metadataGroup);
    m_comment = new QLineEdit(m_metadataGroup);
    m_description->setPlaceholderText(QStringLiteral("Image description"));
    m_artist->setPlaceholderText(QStringLiteral("Creator or photographer"));
    m_copyright->setPlaceholderText(QStringLiteral("Copyright notice"));
    m_comment->setPlaceholderText(QStringLiteral("Comment"));
    metadataLayout->addRow(QStringLiteral("Description"), m_description);
    metadataLayout->addRow(QStringLiteral("Artist"), m_artist);
    metadataLayout->addRow(QStringLiteral("Copyright"), m_copyright);
    metadataLayout->addRow(QStringLiteral("Comment"), m_comment);
    m_metadataGroup->setEnabled(false);
    if (ImageProcessor::metadataExecutable().isEmpty()) {
        m_metadataGroup->setTitle(QStringLiteral("Image attributes (Exiv2 required)"));
        m_metadataGroup->setToolTip(
            QStringLiteral("Install Exiv2 to view and edit image attributes."));
    }
    controlsLayout->addWidget(m_metadataGroup);

    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(new QLabel(QStringLiteral("<b>Zoom</b>"), controls));
    auto *zoomRow = new QHBoxLayout;
    m_zoom = new QSlider(Qt::Horizontal, controls);
    m_zoom->setRange(5, 2000);
    m_zoom->setValue(100);
    zoomRow->addWidget(m_zoom, 1);
    auto *resetZoomButton = new QToolButton(controls);
    resetZoomButton->setText(QStringLiteral("Reset"));
    resetZoomButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    resetZoomButton->setToolTip(QStringLiteral("Reset zoom to 100%"));
    zoomRow->addWidget(resetZoomButton);
    controlsLayout->addLayout(zoomRow);

    controlsLayout->addSpacing(12);
    m_outputQualityHeading = new QLabel(QStringLiteral("<b>Output quality</b>"), controls);
    controlsLayout->addWidget(m_outputQualityHeading);
    auto *outputQualityRow = new QHBoxLayout;
    m_outputQuality = new QSlider(Qt::Horizontal, controls);
    m_outputQuality->setRange(1, 100);
    m_outputQuality->setValue(95);
    m_outputQuality->setEnabled(false);
    m_outputQuality->setToolTip(
        QStringLiteral("JPEG and WebP only. Lower quality usually creates smaller files."));
    outputQualityRow->addWidget(m_outputQuality, 1);
    m_outputQualityValue = new QLabel(QStringLiteral("95%"), controls);
    m_outputQualityValue->setMinimumWidth(38);
    m_outputQualityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_outputQualityValue->setEnabled(false);
    outputQualityRow->addWidget(m_outputQualityValue);
    m_resetQualityButton = new QToolButton(controls);
    m_resetQualityButton->setText(QStringLiteral("Reset"));
    m_resetQualityButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_resetQualityButton->setToolTip(QStringLiteral("Reset output quality to 95%"));
    m_resetQualityButton->setEnabled(false);
    outputQualityRow->addWidget(m_resetQualityButton);
    controlsLayout->addLayout(outputQualityRow);
    m_fileSizeLabel = new QLabel(QStringLiteral("Add an image to compare file sizes."), controls);
    m_fileSizeLabel->setWordWrap(true);
    controlsLayout->addWidget(m_fileSizeLabel);

    m_outputEstimateTimer = new QTimer(this);
    m_outputEstimateTimer->setSingleShot(true);
    m_outputEstimateTimer->setInterval(350);
    connect(m_outputEstimateTimer, &QTimer::timeout, this, [this] {
        if (m_outputEstimateRunning) {
            m_outputEstimateTimer->start();
            return;
        }
        const int row = m_queue->currentRow();
        if (row < 0 || row >= m_jobs.size())
            return;
        const ImageJob job = m_jobs[row];
        const QString suffix = QFileInfo(job.sourcePath).suffix().toLower();
        const quint64 generation = m_outputEstimateGeneration;
        m_outputEstimateRunning = true;
        auto *watcher = new QFutureWatcher<qint64>(this);
        connect(watcher, &QFutureWatcher<qint64>::finished, this,
                [this, watcher, generation, sourcePath = job.sourcePath, suffix] {
            const qint64 estimatedBytes = watcher->result();
            watcher->deleteLater();
            m_outputEstimateRunning = false;
            const int currentRow = m_queue->currentRow();
            if (generation != m_outputEstimateGeneration
                || currentRow < 0 || currentRow >= m_jobs.size()
                || m_jobs[currentRow].sourcePath != sourcePath)
                return;
            const qint64 sourceBytes = QFileInfo(sourcePath).size();
            m_fileSizeLabel->setText(
                QStringLiteral("Original file size: %1 · Estimated %2 output: %3")
                    .arg(formatFileSize(sourceBytes), outputFormatName(suffix),
                         formatFileSize(estimatedBytes)));
        });
        watcher->setFuture(QtConcurrent::run([job, suffix] {
            return ImageProcessor::estimateOutputSize(job, suffix);
        }));
    });

    m_qualityLabel = new QLabel(QStringLiteral("Add an image to see scale information."), controls);
    m_qualityLabel->setWordWrap(true);
    controlsLayout->addWidget(m_qualityLabel);
    controlsLayout->addStretch();

    m_sourceLabel = new QLabel(QStringLiteral("Source: no image selected"), controls);
    m_sourceLabel->setWordWrap(true);
    controlsLayout->addWidget(m_sourceLabel);

    auto *saveAsButton = new QPushButton(QStringLiteral("Save As…"), controls);
    controlsLayout->addWidget(saveAsButton);

    splitter->addWidget(m_queue);
    splitter->addWidget(m_canvas);
    splitter->addWidget(controls);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter);
    setCentralWidget(central);

    connect(m_queue, &QListWidget::currentRowChanged, this, &MainWindow::loadCurrentJob);
    connect(m_queue, &QListWidget::itemSelectionChanged,
            this, &MainWindow::syncResolutionControlsToSelection);
    connect(saveAsButton, &QPushButton::clicked, this, &MainWindow::saveSelectedAs);
    connect(m_width, &QSpinBox::valueChanged,
            this, &MainWindow::applyTargetResolutionToSelection);
    connect(m_height, &QSpinBox::valueChanged,
            this, &MainWindow::applyTargetResolutionToSelection);
    connect(m_zoom, &QSlider::valueChanged,
            this, &MainWindow::updateCurrentZoomFromControl);
    connect(m_outputQuality, &QSlider::valueChanged, this, [this](const int value) {
        m_outputQualityValue->setText(QStringLiteral("%1%").arg(value));
        updateCurrentOutputQuality();
    });
    connect(m_resetQualityButton, &QToolButton::clicked, this, [this] {
        m_outputQuality->setValue(95);
    });
    for (QLineEdit *field : {m_description, m_artist, m_copyright, m_comment}) {
        connect(field, &QLineEdit::textEdited,
                this, &MainWindow::updateCurrentMetadataFromControls);
    }
    connect(resetZoomButton, &QToolButton::clicked, this, [this] {
        m_zoom->setValue(100);
    });
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
        applyTargetResolutionToSelection();
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

    setMenuBarVisible(m_settings.menuBarVisible());
}

void MainWindow::populateResolutionTargets(const bool selectPrimary)
{
    const QSize previousTarget(m_width->value(), m_height->value());
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const QList<QScreen *> screens = QGuiApplication::screens();
    const QSignalBlocker blocker(m_resolution);
    m_resolution->clear();

    const auto addScreen = [this, primaryScreen](QScreen *screen) {
        const QSize pixels = screen->size() * screen->devicePixelRatio();
        const QString primaryLabel = screen == primaryScreen
            ? QStringLiteral(" (Primary)") : QString();
        m_resolution->addItem(QStringLiteral("%1%2 — %3 × %4")
                                  .arg(screen->name())
                                  .arg(primaryLabel)
                                  .arg(pixels.width())
                                  .arg(pixels.height()), pixels);
    };

    if (primaryScreen && screens.contains(primaryScreen))
        addScreen(primaryScreen);
    for (QScreen *screen : screens) {
        if (screen != primaryScreen)
            addScreen(screen);
    }
    for (const QSize &size : m_settings.resolutionPresets()) {
        m_resolution->addItem(QStringLiteral("Preset — %1 × %2")
                                  .arg(size.width()).arg(size.height()), size);
    }

    const QSize primarySize = primaryScreen
        ? primaryScreen->size() * primaryScreen->devicePixelRatio() : QSize();
    const QSize target = selectPrimary && primarySize.isValid()
        ? primarySize : previousTarget;
    int targetIndex = m_resolution->findData(target);
    if (targetIndex < 0 && target.isValid()) {
        m_resolution->insertItem(0, QStringLiteral("Current — %1 × %2")
                                      .arg(target.width()).arg(target.height()), target);
        targetIndex = 0;
    }
    if (targetIndex >= 0)
        m_resolution->setCurrentIndex(targetIndex);

    if (target.isValid()) {
        m_updatingControls = true;
        m_width->setValue(target.width());
        m_height->setValue(target.height());
        m_updatingControls = false;
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
        job.metadata = ImageProcessor::readMetadata(path);
        m_jobs.push_back(job);
        m_queue->addItem(job.displayName());
        queuedPaths.insert(path);
        ++added;
    }
    if (m_queue->currentRow() < 0 && !m_jobs.isEmpty())
        m_queue->setCurrentRow(0);
    statusBar()->showMessage(QStringLiteral("Added %1 image(s) to the queue.").arg(added), 5000);
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
    const QVector<int> selected = selectedJobRows();
    if (selected.isEmpty())
        return;

    QSet<int> selectedRows;
    for (const int row : selected)
        selectedRows.insert(row);
    removeJobRows(selectedRows);
}

QVector<int> MainWindow::selectedJobRows() const
{
    QVector<int> rows;
    const QModelIndexList selected = m_queue->selectionModel()->selectedRows();
    rows.reserve(selected.size());
    for (const QModelIndex &index : selected)
        rows.push_back(index.row());
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty() && m_queue->currentRow() >= 0)
        rows.push_back(m_queue->currentRow());
    return rows;
}

void MainWindow::removeJobRows(const QSet<int> &rows)
{
    if (rows.isEmpty())
        return;
    const int nextRow = *std::min_element(rows.cbegin(), rows.cend());

    QVector<ImageJob> remainingJobs;
    remainingJobs.reserve(m_jobs.size() - rows.size());
    for (int row = 0; row < m_jobs.size(); ++row) {
        if (!rows.contains(row))
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
        m_outputQualityHeading->setText(QStringLiteral("<b>Output quality</b>"));
        m_outputQuality->setEnabled(false);
        m_outputQualityValue->setEnabled(false);
        m_resetQualityButton->setEnabled(false);
        m_metadataGroup->setEnabled(false);
        m_description->clear();
        m_artist->clear();
        m_copyright->clear();
        m_comment->clear();
        updateQualityLabel();
    } else {
        loadCurrentJob(m_queue->currentRow());
    }
}

void MainWindow::showQueueContextMenu(const QPoint &position)
{
    QListWidgetItem *clicked = m_queue->itemAt(position);
    if (!clicked)
        return;
    if (!clicked->isSelected()) {
        m_queue->clearSelection();
        clicked->setSelected(true);
        m_queue->setCurrentItem(clicked);
    }

    QMenu menu(this);
    menu.addAction(QStringLiteral("Save As…"), this, &MainWindow::saveSelectedAs);
    menu.addSeparator();
    menu.addAction(QStringLiteral("Move to Trash"), this,
                   &MainWindow::moveSelectedImagesToTrash);
    menu.addAction(QStringLiteral("Remove from Queue"), this,
                   &MainWindow::removeSelectedImages);
    menu.exec(m_queue->viewport()->mapToGlobal(position));
}

void MainWindow::moveSelectedImagesToTrash()
{
    const QVector<int> rows = selectedJobRows();
    if (rows.isEmpty())
        return;
    if (QMessageBox::warning(
            this, QStringLiteral("Move wallpapers to Trash"),
            QStringLiteral("Move %1 selected wallpaper(s) to the desktop Trash?\n\n"
                           "This removes the source files, not just the queue entries.")
                .arg(rows.size()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;

    QSet<int> removed;
    QStringList failures;
    for (const int row : rows) {
        if (row < 0 || row >= m_jobs.size())
            continue;
        const QString path = m_jobs[row].sourcePath;
        if (QFile::moveToTrash(path))
            removed.insert(row);
        else
            failures << QStringLiteral("%1: could not be moved to Trash")
                            .arg(QFileInfo(path).fileName());
    }
    removeJobRows(removed);
    statusBar()->showMessage(QStringLiteral("Moved %1 wallpaper(s) to Trash.")
                                 .arg(removed.size()), 5000);
    if (!failures.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Some files were not moved"),
                             failures.join('\n'));
}

void MainWindow::saveAsCurrent()
{
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;
    const QFileInfo source(m_jobs[row].sourcePath);
    const QString initialFolder = m_settings.lastSaveAsFolder().isEmpty()
        ? source.absolutePath() : m_settings.lastSaveAsFolder();
    QString destination = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save processed image as"),
        QDir(initialFolder).filePath(source.fileName()),
        QStringLiteral("Images (*.jpg *.jpeg *.png *.webp)"), nullptr,
        QFileDialog::DontConfirmOverwrite);
    if (destination.isEmpty())
        return;

    DestinationNumbering numbering = inspectDestinationNumbering(
        QFileInfo(destination).absolutePath());
    bool useSequentialNumbering = false;
    if (!chooseSequentialNumbering(this, numbering, useSequentialNumbering))
        return;
    if (useSequentialNumbering) {
        qulonglong nextNumber = 0;
        if (!takeNextAvailableNumber(numbering, nextNumber)) {
            QMessageBox::warning(this, QStringLiteral("File numbering unavailable"),
                                 QStringLiteral("No numeric filename is available."));
            return;
        }
        const QFileInfo selectedDestination(destination);
        const QString stem = QString::number(nextNumber).rightJustified(
            numbering.paddingWidth, QLatin1Char('0'));
        const QString outputName = selectedDestination.suffix().isEmpty()
            ? stem : stem + QLatin1Char('.') + selectedDestination.suffix();
        destination = selectedDestination.dir().filePath(outputName);
    }
    if (QFileInfo(destination).absoluteFilePath() == source.absoluteFilePath()) {
        QMessageBox::information(this, QStringLiteral("Choose another destination"),
            QStringLiteral("Save As never replaces the original image. Choose a different "
                           "filename or folder."));
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
    const ProcessingRequest request{m_jobs[row], destination, overwrite};
    if (!confirmExportMappings(this, {request}))
        return;
    statusBar()->showMessage(QStringLiteral("Saving copy…"));
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

void MainWindow::saveSelectedAs()
{
    const QVector<int> rows = selectedJobRows();
    if (rows.isEmpty())
        return;

    if (rows.size() <= 1) {
        saveAsCurrent();
        return;
    }

    const QString initialFolder = m_settings.lastSaveAsFolder().isEmpty()
        ? QFileInfo(m_jobs[rows.first()].sourcePath).absolutePath()
        : m_settings.lastSaveAsFolder();
    const QString folder = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose export folder"), initialFolder);
    if (folder.isEmpty())
        return;

    QVector<ProcessingRequest> requests;
    QStringList skipped;
    QSet<QString> plannedDestinations;
    DestinationNumbering numbering = inspectDestinationNumbering(folder);
    bool useSequentialNumbering = false;
    if (!chooseSequentialNumbering(this, numbering, useSequentialNumbering))
        return;
    bool cancelled = false;
    for (const int row : rows) {
        if (row < 0 || row >= m_jobs.size())
            continue;
        const QFileInfo source(m_jobs[row].sourcePath);
        QString outputName = source.fileName();
        if (useSequentialNumbering) {
            qulonglong nextNumber = 0;
            if (!takeNextAvailableNumber(numbering, nextNumber)) {
                skipped << QStringLiteral("%1: destination numbering is exhausted")
                               .arg(source.fileName());
                continue;
            }
            const QString stem = QString::number(nextNumber).rightJustified(
                numbering.paddingWidth, QLatin1Char('0'));
            outputName = source.suffix().isEmpty()
                ? stem : stem + QLatin1Char('.') + source.suffix();
        }
        const QString destination = QDir(folder).filePath(outputName);
        const QString absoluteDestination = QFileInfo(destination).absoluteFilePath();
        if (absoluteDestination == source.absoluteFilePath()) {
            skipped << QStringLiteral("%1: destination is the original source")
                           .arg(source.fileName());
            continue;
        }

        bool overwrite = false;
        if (QFileInfo::exists(destination)
            || plannedDestinations.contains(absoluteDestination)) {
            QMessageBox collision(this);
            collision.setIcon(QMessageBox::Question);
            collision.setWindowTitle(QStringLiteral("Replace existing file?"));
            collision.setText(QStringLiteral("%1 conflicts with another export.")
                                  .arg(destination));
            collision.setInformativeText(
                QStringLiteral("Replace it, skip this image, or cancel the remaining batch?"));
            collision.setStandardButtons(
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            collision.setDefaultButton(QMessageBox::No);
            const int answer = collision.exec();
            if (answer == QMessageBox::Cancel) {
                cancelled = true;
                break;
            }
            if (answer == QMessageBox::No) {
                skipped << QStringLiteral("%1: conflicting destination skipped")
                               .arg(source.fileName());
                continue;
            }
            overwrite = true;
        }
        requests.push_back({m_jobs[row], destination, overwrite});
        plannedDestinations.insert(absoluteDestination);
    }
    if (cancelled)
        skipped << QStringLiteral("Remaining exports were cancelled.");
    if (requests.isEmpty()) {
        if (!skipped.isEmpty())
            QMessageBox::information(this, QStringLiteral("Nothing exported"),
                                     skipped.join('\n'));
        return;
    }

    if (!confirmExportMappings(this, requests))
        return;

    statusBar()->showMessage(QStringLiteral("Saving %1 image(s)…").arg(requests.size()));
    auto *watcher = new QFutureWatcher<QVector<ProcessingResult>>(this);
    connect(watcher, &QFutureWatcher<QVector<ProcessingResult>>::finished, this,
            [this, watcher, folder, skipped] {
        const QVector<ProcessingResult> results = watcher->result();
        watcher->deleteLater();
        int succeeded = 0;
        QStringList failures;
        for (const ProcessingResult &result : results) {
            if (result.succeeded)
                ++succeeded;
            else
                failures << QStringLiteral("%1: %2")
                                .arg(QFileInfo(result.sourcePath).fileName(),
                                     result.errorMessage);
        }
        m_settings.setLastSaveAsFolder(folder);
        statusBar()->showMessage(QStringLiteral("Saved %1 of %2 selected image(s).")
                                     .arg(succeeded).arg(results.size()), 10000);
        QStringList report = skipped;
        report.append(failures);
        if (!report.isEmpty())
            QMessageBox::warning(this, QStringLiteral("Batch export report"),
                                 report.join('\n'));
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

void MainWindow::setMenuBarVisible(const bool visible)
{
    menuBar()->setVisible(visible);
    if (m_toggleMenuAction->isChecked() != visible)
        m_toggleMenuAction->setChecked(visible);
    m_settings.setMenuBarVisible(visible);
}

void MainWindow::showHelp()
{
    QFile helpFile(QStringLiteral(":/docs/HELP.md"));
    if (!helpFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Help unavailable"),
                             QStringLiteral("The LGL Papercutter help file could not be opened."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("LGL Papercutter Help"));
    dialog.resize(780, 680);
    auto *layout = new QVBoxLayout(&dialog);
    auto *browser = new QTextBrowser(&dialog);
    browser->setOpenExternalLinks(true);
    browser->setMarkdown(QString::fromUtf8(helpFile.readAll()));
    layout->addWidget(browser);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showAbout()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("About LGL Papercutter"));
    dialog.setMinimumWidth(500);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);

    auto *heading = new QHBoxLayout;
    auto *icon = new QLabel(&dialog);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/lgl-papercutter.png"))
                        .pixmap(QSize(96, 96)));
    icon->setAlignment(Qt::AlignTop);
    heading->addWidget(icon);

    auto *identity = new QLabel(
        QStringLiteral("<h2>LGL Papercutter</h2>"
                       "<p><b>Version %1</b></p>"
                       "<p>Compose wallpapers for your display resolution without "
                       "altering the original images.</p>")
            .arg(QApplication::applicationVersion()),
        &dialog);
    identity->setWordWrap(true);
    heading->addWidget(identity, 1);
    layout->addLayout(heading);

    auto *details = new QLabel(
        QStringLiteral("<p>Images are processed locally and exported as new files. "
                       "Built with Qt 6 and ImageMagick.</p>"
                       "<p><a href=\"https://github.com/linuxgamerlife/lgl-papercutter\">"
                       "Project page and issue tracker</a></p>"
                       "<p>Made by <a href=\"https://www.youtube.com/@linuxgamerlife\">"
                       "LinuxGamerLife</a><br>"
                       "<a href=\"https://ko-fi.com/G2G3V70LW\">Support development on Ko-fi</a>"
                       "<br>Released under the MIT License.</p>"),
        &dialog);
    details->setWordWrap(true);
    details->setOpenExternalLinks(true);
    details->setTextInteractionFlags(Qt::TextBrowserInteraction);
    layout->addWidget(details);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
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
            const QString hash = fileSha256(jobs[row].sourcePath);
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
        m_outputQualityHeading->setText(QStringLiteral("<b>Output quality</b>"));
        m_outputQuality->setEnabled(false);
        m_outputQualityValue->setEnabled(false);
        m_resetQualityButton->setEnabled(false);
        m_metadataGroup->setEnabled(false);
        m_description->clear();
        m_artist->clear();
        m_copyright->clear();
        m_comment->clear();
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
    m_outputQuality->setValue(job.outputQuality);
    const QString suffix = QFileInfo(job.sourcePath).suffix().toLower();
    const bool qualityApplies = suffix != QStringLiteral("png");
    m_outputQualityHeading->setText(qualityApplies
        ? QStringLiteral("<b>Output quality (%1)</b>").arg(outputFormatName(suffix))
        : QStringLiteral("<b>PNG compression</b>"));
    m_outputQuality->setEnabled(qualityApplies);
    m_outputQualityValue->setEnabled(qualityApplies);
    m_resetQualityButton->setEnabled(qualityApplies);
    m_description->setText(job.metadata.description);
    m_artist->setText(job.metadata.artist);
    m_copyright->setText(job.metadata.copyright);
    m_comment->setText(job.metadata.comment);
    m_metadataGroup->setEnabled(!ImageProcessor::metadataExecutable().isEmpty());
    m_updatingControls = false;
    syncResolutionControlsToSelection();
    updateQualityLabel();
}

void MainWindow::applyTargetResolutionToSelection()
{
    if (m_updatingControls)
        return;
    const QVector<int> rows = selectedJobRows();
    if (rows.isEmpty())
        return;

    const QSize targetSize(m_width->value(), m_height->value());
    for (const int row : rows) {
        if (row >= 0 && row < m_jobs.size())
            m_jobs[row].composition.targetSize = targetSize;
    }
    const int currentRow = m_queue->currentRow();
    if (currentRow >= 0 && currentRow < m_jobs.size())
        m_canvas->setComposition(m_jobs[currentRow].composition);
    syncResolutionControlsToSelection();
    updateQualityLabel();
}

void MainWindow::updateCurrentZoomFromControl()
{
    if (m_updatingControls)
        return;
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;

    m_jobs[row].composition.zoom = m_zoom->value() / 100.0;
    m_canvas->setComposition(m_jobs[row].composition);
    updateQualityLabel();
}

void MainWindow::updateCurrentOutputQuality()
{
    if (m_updatingControls)
        return;
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;
    m_jobs[row].outputQuality = m_outputQuality->value();
    updateQualityLabel();
}

void MainWindow::updateCurrentMetadataFromControls()
{
    if (m_updatingControls)
        return;
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;

    ImageJob &job = m_jobs[row];
    job.metadata.description = m_description->text();
    job.metadata.artist = m_artist->text();
    job.metadata.copyright = m_copyright->text();
    job.metadata.comment = m_comment->text();
    job.metadataEdited = true;
}

void MainWindow::syncResolutionControlsToSelection()
{
    if (m_updatingControls || !m_resolution)
        return;

    const QVector<int> rows = selectedJobRows();
    const int currentRow = m_queue->currentRow();
    if (rows.isEmpty() || currentRow < 0 || currentRow >= m_jobs.size())
        return;

    const QSize currentSize = m_jobs[currentRow].composition.targetSize;
    bool mixed = false;
    const QSize firstSize = m_jobs[rows.first()].composition.targetSize;
    for (const int row : rows) {
        if (row >= 0 && row < m_jobs.size()
            && m_jobs[row].composition.targetSize != firstSize) {
            mixed = true;
            break;
        }
    }

    m_updatingControls = true;
    for (int index = m_resolution->count() - 1; index >= 0; --index) {
        if (m_resolution->itemData(index, Qt::UserRole + 1).toBool())
            m_resolution->removeItem(index);
    }

    if (mixed) {
        m_resolution->insertItem(0, QStringLiteral("Mixed resolutions"), QVariant());
        m_resolution->setItemData(0, true, Qt::UserRole + 1);
        m_resolution->setCurrentIndex(0);
    } else {
        int index = m_resolution->findData(firstSize);
        if (index < 0) {
            m_resolution->insertItem(
                0, QStringLiteral("Staged — %1 × %2")
                       .arg(firstSize.width()).arg(firstSize.height()), firstSize);
            m_resolution->setItemData(0, true, Qt::UserRole + 1);
            index = 0;
        }
        m_resolution->setCurrentIndex(index);
    }
    m_width->setValue(currentSize.width());
    m_height->setValue(currentSize.height());
    m_updatingControls = false;
}

void MainWindow::updateQualityLabel()
{
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size()) {
        m_qualityLabel->setText(QStringLiteral("Add an image to see scale information."));
        m_fileSizeLabel->setText(QStringLiteral("Add an image to compare file sizes."));
        ++m_outputEstimateGeneration;
        m_outputEstimateTimer->stop();
        return;
    }

    const ImageJob &job = m_jobs[row];
    const double scale = job.composition.effectiveScaleFor(job.sourceSize);
    const QString warning = scale > 1.0
        ? QStringLiteral(" — enlargement may look soft") : QString();
    m_qualityLabel->setText(QStringLiteral("Effective scale: %1×%2")
                                .arg(scale, 0, 'f', 2).arg(warning));
    scheduleOutputSizeEstimate();
}

void MainWindow::scheduleOutputSizeEstimate()
{
    const int row = m_queue->currentRow();
    if (row < 0 || row >= m_jobs.size())
        return;
    ++m_outputEstimateGeneration;
    const ImageJob &job = m_jobs[row];
    const QString suffix = QFileInfo(job.sourcePath).suffix().toLower();
    m_fileSizeLabel->setText(
        QStringLiteral("Original file size: %1 · Estimated %2 output: calculating…")
            .arg(formatFileSize(QFileInfo(job.sourcePath).size()),
                 outputFormatName(suffix)));
    m_outputEstimateTimer->start();
}

} // namespace papercutter
