#include "MainWindow.h"
#include "Version.h"
#include "FPMReader.h"
#include "FPMWriter.h"
#include "AssetManager.h"
#include "MemoryAnalyzerDialog.h"
#include "MemoryAnalyzer.h"
#include "PortalLeakDialog.h"
#include "ZoneVisibilityDialog.h"
#include <tuple>
#include <set>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QApplication>
#include <QCloseEvent>
#include <QToolButton>
#include <QPainterPath>
#include <QPainter>
#include <QPolygonF>
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QDebug>

static QIcon makeGhostFloorIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Lower ghost floor plane (dashed outline)
    QPolygonF lowerPlane;
    lowerPlane << QPointF(2, 13) << QPointF(10, 9) << QPointF(18, 13) << QPointF(10, 17);
    p.setPen(QPen(QColor(110, 125, 150, 180), 1.2f, Qt::DashLine));
    p.setBrush(QColor(40, 50, 70, 100));
    p.drawPolygon(lowerPlane);

    // Upper active floor plane (crisp outline, translucent fill)
    QPolygonF upperPlane;
    upperPlane << QPointF(2, 6) << QPointF(10, 2) << QPointF(18, 6) << QPointF(10, 10);
    p.setPen(QPen(QColor(170, 195, 230), 1.4f));
    p.setBrush(QColor(60, 85, 125, 180));
    p.drawPolygon(upperPlane);

    return QIcon(px);
}

static QIcon makePortalIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Outer portal frame
    p.setPen(QPen(QColor(130, 190, 240), 1.4f));
    p.setBrush(QBrush(QColor(35, 60, 95, 160)));
    p.drawEllipse(QRectF(4, 2, 12, 16));

    // Inner portal core
    p.setPen(QPen(QColor(210, 235, 255), 1.0f));
    p.setBrush(QBrush(QColor(70, 145, 220, 180)));
    p.drawEllipse(QRectF(7, 5, 6, 10));

    return QIcon(px);
}

static QIcon makeMemoryIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // PCB board (dark emerald/teal circuit board)
    QRectF pcb(2, 4, 16, 11);
    p.setPen(QPen(QColor(46, 125, 88), 1.2f));
    p.setBrush(QColor(22, 58, 42));
    p.drawRoundedRect(pcb, 1.5, 1.5);

    // Gold contact pins at bottom edge with DIMM key notch
    p.setPen(QPen(QColor(230, 185, 65), 1.2f));
    for (float x = 3.5f; x <= 8.5f; x += 1.5f) {
        p.drawLine(QPointF(x, 13.0f), QPointF(x, 15.0f));
    }
    for (float x = 11.5f; x <= 16.5f; x += 1.5f) {
        p.drawLine(QPointF(x, 13.0f), QPointF(x, 15.0f));
    }

    // 3 Memory DRAM IC chips
    p.setPen(QPen(QColor(30, 38, 50), 1.0f));
    p.setBrush(QColor(16, 20, 26));
    p.drawRect(QRectF(3.5, 6.0, 3.0, 5.5));
    p.drawRect(QRectF(8.5, 6.0, 3.0, 5.5));
    p.drawRect(QRectF(13.5, 6.0, 3.0, 5.5));

    // Chip pin solder accents
    p.setPen(QPen(QColor(160, 185, 210, 190), 1.0f));
    p.drawLine(QPointF(4.0, 5.5), QPointF(6.0, 5.5));
    p.drawLine(QPointF(9.0, 5.5), QPointF(11.0, 5.5));
    p.drawLine(QPointF(14.0, 5.5), QPointF(16.0, 5.5));

    return QIcon(px);
}

static QIcon makeColorZonesIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw 3 overlapping rounded color swatches representing colored zones
    // 1. Magenta / Pink swatch (top-left)
    p.setPen(QPen(QColor(245, 70, 160), 1.2f));
    p.setBrush(QColor(230, 50, 140, 210));
    p.drawRoundedRect(QRectF(2.0, 2.0, 8.5, 8.5), 2.0, 2.0);

    // 2. Cyan swatch (top-right)
    p.setPen(QPen(QColor(40, 220, 245), 1.2f));
    p.setBrush(QColor(20, 190, 220, 210));
    p.drawRoundedRect(QRectF(9.5, 2.0, 8.5, 8.5), 2.0, 2.0);

    // 3. Amber / Gold swatch (bottom-center)
    p.setPen(QPen(QColor(255, 195, 45), 1.2f));
    p.setBrush(QColor(245, 165, 25, 210));
    p.drawRoundedRect(QRectF(5.5, 9.0, 9.0, 8.5), 2.0, 2.0);

    return QIcon(px);
}

static QIcon makeTracerIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Cyan eye outline
    p.setPen(QPen(QColor(56, 189, 248), 1.6f));
    p.setBrush(QBrush(QColor(15, 23, 42, 180)));
    QPainterPath eyePath;
    eyePath.moveTo(2.0, 10.0);
    eyePath.quadTo(10.0, 3.0, 18.0, 10.0);
    eyePath.quadTo(10.0, 17.0, 2.0, 10.0);
    p.drawPath(eyePath);

    // Pupil
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(14, 165, 233));
    p.drawEllipse(QPointF(10.0, 10.0), 3.5, 3.5);

    p.setBrush(QColor(240, 249, 255));
    p.drawEllipse(QPointF(11.0, 9.0), 1.2, 1.2);

    // Laser / Ray beam extending from pupil
    p.setPen(QPen(QColor(239, 68, 68), 1.5f, Qt::DashLine));
    p.drawLine(QPointF(13.0, 10.0), QPointF(19.0, 10.0));

    return QIcon(px);
}

static QIcon makeReloadIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Circular arrow
    p.setPen(QPen(QColor(160, 200, 240), 1.8f));
    QRectF arcRect(3.5, 3.5, 13.0, 13.0);
    p.drawArc(arcRect, 45 * 16, 270 * 16);

    // Arrowhead at top
    QPolygonF head;
    head << QPointF(11.5, 1.0) << QPointF(16.5, 5.0) << QPointF(11.5, 9.0);
    p.setBrush(QColor(160, 200, 240));
    p.setPen(Qt::NoPen);
    p.drawPolygon(head);

    return QIcon(px);
}

static QIcon makeZoomFitIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(160, 200, 240), 1.8f));
    // 4 corner brackets indicating framing / fit to view
    // Top-Left
    p.drawLine(QPointF(3.5, 7.5), QPointF(3.5, 3.5));
    p.drawLine(QPointF(3.5, 3.5), QPointF(7.5, 3.5));

    // Top-Right
    p.drawLine(QPointF(12.5, 3.5), QPointF(16.5, 3.5));
    p.drawLine(QPointF(16.5, 3.5), QPointF(16.5, 7.5));

    // Bottom-Left
    p.drawLine(QPointF(3.5, 12.5), QPointF(3.5, 16.5));
    p.drawLine(QPointF(3.5, 16.5), QPointF(7.5, 16.5));

    // Bottom-Right
    p.drawLine(QPointF(12.5, 16.5), QPointF(16.5, 16.5));
    p.drawLine(QPointF(16.5, 16.5), QPointF(16.5, 12.5));

    // Center indicator dot
    p.setBrush(QColor(160, 200, 240));
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(8.5, 8.5, 3.0, 3.0));

    return QIcon(px);
}

static QIcon makeEntityToggleIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Stylized isometric 3D cube / entity marker in emerald green
    p.setPen(QPen(QColor(80, 220, 140), 1.4f));
    // Top face
    QPolygonF top;
    top << QPointF(10.0, 3.0) << QPointF(16.0, 6.5) << QPointF(10.0, 10.0) << QPointF(4.0, 6.5);
    p.setBrush(QColor(60, 200, 120, 180));
    p.drawPolygon(top);

    // Left face
    QPolygonF left;
    left << QPointF(4.0, 6.5) << QPointF(10.0, 10.0) << QPointF(10.0, 16.5) << QPointF(4.0, 13.0);
    p.setBrush(QColor(40, 170, 95, 210));
    p.drawPolygon(left);

    // Right face
    QPolygonF right;
    right << QPointF(10.0, 10.0) << QPointF(16.0, 6.5) << QPointF(16.0, 13.0) << QPointF(10.0, 16.5);
    p.setBrush(QColor(30, 145, 80, 230));
    p.drawPolygon(right);

    return QIcon(px);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    updateWindowTitle();
    resize(1360, 860);

    QIcon appIcon(":/app.ico");
    appIcon.addFile(":/app.png");
    setWindowIcon(appIcon);

    m_canvas = new MapCanvas(this);
    setCentralWidget(m_canvas);

    m_visZoneManager = std::make_shared<VisZoneManager>();
    m_canvas->setVisZoneManager(m_visZoneManager);

    // Left Dock: Entity Search (full height)
    m_searchDock = new EntitySearchDock(this);
    m_searchDock->setMinimumWidth(260);
    addDockWidget(Qt::LeftDockWidgetArea, m_searchDock);

    // Right Dock 1: Entity Inspector
    m_inspectorDock = new EntityInspector(this);
    m_inspectorDock->setMinimumWidth(330);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);

    // Right Dock 2: Visibility Zones & Portals (PVS) with unified Tracer tab
    m_visZoneDock = new VisZoneDock(this);
    m_visZoneDock->setVisZoneManager(m_visZoneManager);
    m_visZoneDock->setMinimumWidth(330);
    addDockWidget(Qt::RightDockWidgetArea, m_visZoneDock);

    // Status bar setup
    m_statusMapName = new QLabel(QStringLiteral("No map loaded"), this);
    m_statusFloor = new QLabel(QStringLiteral("Floor: 0"), this);
    m_statusCoords = new QLabel(QStringLiteral("Tile: (0, 0)"), this);
    m_statusMemory = new QLabel(QStringLiteral("RAM: 0 MB"), this);

    statusBar()->addWidget(m_statusMapName, 2);
    statusBar()->addWidget(m_statusFloor, 1);
    statusBar()->addWidget(m_statusCoords, 2);
    statusBar()->addPermanentWidget(m_statusMemory, 1);

    createMenusAndToolbars();

    // Signal / Slot Wiring
    connect(m_canvas, &MapCanvas::floorChanged, this, &MainWindow::onCanvasFloorChanged);
    connect(m_canvas, &MapCanvas::entitySelected, this, &MainWindow::onEntitySelected);
    connect(m_canvas, &MapCanvas::hoverInfoChanged, this, &MainWindow::onHoverInfoChanged);
    connect(m_canvas, &MapCanvas::zoomChanged, this, &MainWindow::onZoomChanged);

    connect(m_searchDock, &EntitySearchDock::entitySelected, this, &MainWindow::onEntitySelected);
    connect(m_searchDock, &EntitySearchDock::focusEntityRequested, this, &MainWindow::onFocusEntityRequested);
    connect(m_searchDock, &EntitySearchDock::entityDeleteRequested, this, &MainWindow::deleteEntity);
    connect(m_inspectorDock, &EntityInspector::entityModified, this, &MainWindow::onEntityModified);
    connect(m_canvas, &MapCanvas::entityModified, this, &MainWindow::onEntityModified);
    connect(m_canvas, &MapCanvas::entityDeleteRequested, this, &MainWindow::deleteEntity);

    connect(m_visZoneDock, &VisZoneDock::zoneSelected, m_canvas, &MapCanvas::setActiveVisZone);
    connect(m_visZoneDock, &VisZoneDock::isolationChanged, m_canvas, &MapCanvas::setVisZoneCulling);
    connect(m_visZoneDock, &VisZoneDock::colorAllZonesToggled, m_canvas, &MapCanvas::setColorAllVisZones);
    connect(m_visZoneDock, &VisZoneDock::entitySelected, this, &MainWindow::onEntitySelected);
    connect(m_visZoneDock, &VisZoneDock::dichotomyDeleteZoneRequested, this, &MainWindow::deleteVisZone);
    connect(m_visZoneDock, &VisZoneDock::traceVisibilityRequested, this, &MainWindow::onOpenZoneVisibilityTracer);
    connect(m_visZoneDock, &VisZoneDock::cellSelected, m_canvas, &MapCanvas::highlightCell);
    connect(m_visZoneDock, &VisZoneDock::tracePathSelected, m_canvas, &MapCanvas::setTracePath);
    connect(m_visZoneDock, &VisZoneDock::portalHighlighted, m_canvas, &MapCanvas::setHighlightedPortal);
    connect(m_visZoneDock, &VisZoneDock::portalHighlightCleared, m_canvas, &MapCanvas::clearHighlightedPortal);
    connect(m_visZoneDock, &VisZoneDock::editSegmentRequested, this, [this](int l, int x, int y) {
        onOpenSegmentEditor(l, x, y);
    });
    connect(m_canvas, &MapCanvas::visZoneSelected, m_visZoneDock, &VisZoneDock::onExternalZoneSelected);
    connect(m_canvas, &MapCanvas::dichotomyDeleteZoneRequested, this, &MainWindow::deleteVisZone);
    connect(m_canvas, &MapCanvas::traceVisibilityRequested, this, &MainWindow::onOpenZoneVisibilityTracer);
    connect(m_canvas, &MapCanvas::segmentInspectRequested, this, [this](int l, int x, int y) {
        onOpenSegmentEditor(l, x, y);
    });
    m_visZoneDock->onIsolationOptionChanged();
}

void MainWindow::createMenusAndToolbars() {
    // -------------------------------------------------------------
    // Menu Bar
    // -------------------------------------------------------------
    m_fileMenu = menuBar()->addMenu(QString());
    m_actOpen = m_fileMenu->addAction(QString(), this, &MainWindow::onOpenMap, QKeySequence::Open);
    m_actSave = m_fileMenu->addAction(QString(), this, &MainWindow::onSaveMap, QKeySequence::Save);
    m_actSaveAs = m_fileMenu->addAction(QString(), this, &MainWindow::onSaveMapAs, QKeySequence::SaveAs);
    m_actReload = m_fileMenu->addAction(QString(), this, &MainWindow::onReloadMap, QKeySequence::Refresh);

    m_recentMapsMenu = m_fileMenu->addMenu(QString());
    populateRecentMapsMenu();

    m_fileMenu->addSeparator();
    m_actConfigEngine = m_fileMenu->addAction(QString(), this, &MainWindow::onConfigureEnginePath);
    m_fileMenu->addSeparator();
    m_actExit = m_fileMenu->addAction(QString(), this, &MainWindow::close, QKeySequence::Quit);

    m_viewMenu = menuBar()->addMenu(QString());
    m_actZoomIn = m_viewMenu->addAction(QString(), m_canvas, &MapCanvas::zoomIn, QKeySequence::ZoomIn);
    m_actZoomOut = m_viewMenu->addAction(QString(), m_canvas, &MapCanvas::zoomOut, QKeySequence::ZoomOut);
    m_actZoomReset = m_viewMenu->addAction(QString(), m_canvas, &MapCanvas::zoomReset, Qt::Key_0);
    m_actZoomFit = m_viewMenu->addAction(QString(), m_canvas, &MapCanvas::zoomFit, Qt::Key_Home);

    m_viewMenu->addSeparator();
    m_actWallTex = m_viewMenu->addAction(QString());
    m_actWallTex->setCheckable(true);
    m_actWallTex->setChecked(true);
    connect(m_actWallTex, &QAction::toggled, m_canvas, &MapCanvas::setShowWallTextures);

    m_actFloorTex = m_viewMenu->addAction(QString());
    m_actFloorTex->setCheckable(true);
    m_actFloorTex->setChecked(true);
    connect(m_actFloorTex, &QAction::toggled, m_canvas, &MapCanvas::setShowFloorTextures);

    m_actGrid = m_viewMenu->addAction(QString());
    m_actGrid->setCheckable(true);
    m_actGrid->setChecked(true);
    connect(m_actGrid, &QAction::toggled, m_canvas, &MapCanvas::setShowGrid);

    m_actEntities = m_viewMenu->addAction(QString());
    m_actEntities->setCheckable(true);
    m_actEntities->setChecked(true);
    connect(m_actEntities, &QAction::toggled, m_canvas, &MapCanvas::setShowEntities);

    m_actLights = m_viewMenu->addAction(QString());
    m_actLights->setCheckable(true);
    m_actLights->setChecked(true);
    connect(m_actLights, &QAction::toggled, m_canvas, &MapCanvas::setShowLights);

    m_actZones = m_viewMenu->addAction(QString());
    m_actZones->setCheckable(true);
    m_actZones->setChecked(true);
    connect(m_actZones, &QAction::toggled, m_canvas, &MapCanvas::setShowZones);

    m_actWaypoints = m_viewMenu->addAction(QString());
    m_actWaypoints->setCheckable(true);
    m_actWaypoints->setChecked(true);
    connect(m_actWaypoints, &QAction::toggled, m_canvas, &MapCanvas::setShowWaypoints);

    m_actGhostLayer = m_viewMenu->addAction(QString());
    m_actGhostLayer->setIcon(makeGhostFloorIcon());
    m_actGhostLayer->setCheckable(true);
    m_actGhostLayer->setChecked(true);
    connect(m_actGhostLayer, &QAction::toggled, m_canvas, &MapCanvas::setShowGhostLayer);

    m_actShowPortals = m_viewMenu->addAction(QString());
    m_actShowPortals->setIcon(makePortalIcon());
    m_actShowPortals->setCheckable(true);
    m_actShowPortals->setChecked(true);
    connect(m_actShowPortals, &QAction::toggled, m_canvas, &MapCanvas::setShowPortals);

    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_searchDock->toggleViewAction());
    m_viewMenu->addAction(m_inspectorDock->toggleViewAction());
    m_viewMenu->addAction(m_visZoneDock->toggleViewAction());

    m_portalsMenu = menuBar()->addMenu(QString());
    m_actPvsPanel = m_portalsMenu->addAction(QString(), this, &MainWindow::onOpenVisZoneDock, QKeySequence(Qt::CTRL + Qt::Key_P));
    m_actResetView = m_portalsMenu->addAction(QString(), m_visZoneDock, &VisZoneDock::resetToNormalView, QKeySequence(Qt::Key_Escape));
    m_portalsMenu->addSeparator();

    m_actColorAllZones = m_portalsMenu->addAction(makeColorZonesIcon(), QString());
    m_actColorAllZones->setCheckable(true);
    m_actColorAllZones->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
    connect(m_actColorAllZones, &QAction::toggled, m_canvas, &MapCanvas::setColorAllVisZones);
    connect(m_actColorAllZones, &QAction::toggled, m_visZoneDock, &VisZoneDock::setColorAllZones);
    connect(m_visZoneDock, &VisZoneDock::colorAllZonesToggled, m_actColorAllZones, &QAction::setChecked);
    m_viewMenu->addAction(m_actColorAllZones);

    m_portalsMenu->addSeparator();
    m_portalsMenu->addAction(m_actShowPortals);
    m_portalsMenu->addSeparator();
    m_actDichotomyDelete = m_portalsMenu->addAction(QString(), this, [this]() { deleteVisZone(); }, QKeySequence(Qt::SHIFT + Qt::Key_Delete));

    m_toolsMenu = menuBar()->addMenu(QString());
    m_actMemoryAnalyzer = m_toolsMenu->addAction(QString(), this, &MainWindow::onOpenMemoryAnalyzer, QKeySequence(Qt::CTRL + Qt::Key_M));
    m_actLeakDetector = m_toolsMenu->addAction(QString(), this, &MainWindow::onOpenPortalLeakDetector);
    m_actTraceVisibility = m_toolsMenu->addAction(makeTracerIcon(), QString(), this, [this]() { onOpenZoneVisibilityTracer(); }, QKeySequence(Qt::CTRL + Qt::Key_T));
    m_actSegmentEditor = m_toolsMenu->addAction(QString(), this, [this]() { onOpenSegmentEditor(); }, QKeySequence(Qt::CTRL + Qt::Key_E));
    m_toolsMenu->addSeparator();
    m_toolsMenu->addAction(m_actDichotomyDelete);
    m_portalsMenu->addAction(m_actTraceVisibility);

    // Language Menu
    m_languageMenu = menuBar()->addMenu(QString());
    QActionGroup* langGroup = new QActionGroup(this);
    langGroup->setExclusive(true);

    m_actLangAuto = m_languageMenu->addAction(QString());
    m_actLangAuto->setCheckable(true);
    langGroup->addAction(m_actLangAuto);

    m_actLangEn = m_languageMenu->addAction(QStringLiteral("English"));
    m_actLangEn->setCheckable(true);
    langGroup->addAction(m_actLangEn);

    m_actLangRu = m_languageMenu->addAction(QStringLiteral("Русский"));
    m_actLangRu->setCheckable(true);
    langGroup->addAction(m_actLangRu);

    connect(m_actLangAuto, &QAction::triggered, this, []() {
        LanguageManager::instance().setLanguage(LanguageManager::Language::Auto);
    });
    connect(m_actLangEn, &QAction::triggered, this, []() {
        LanguageManager::instance().setLanguage(LanguageManager::Language::English);
    });
    connect(m_actLangRu, &QAction::triggered, this, []() {
        LanguageManager::instance().setLanguage(LanguageManager::Language::Russian);
    });

    m_helpMenu = menuBar()->addMenu(QString());
    m_actAbout = m_helpMenu->addAction(QString(), this, [this]() {
        QMessageBox::about(this, tr("About %1").arg(VersionInfo::shortTitle()),
            tr("<h3>%1 v%2 - %3</h3>"
               "<p style='font-size: 13px;'>"
               "<b>Version:</b> %2<br>"
               "<b>Developer:</b> %4<br>"
               "<b>Studio:</b> %5</p>"
               "<hr>"
               "<p>A professional tool for editing, visualizing, and analyzing <b>FPS Creator</b> maps (.FPM).</p>"
               "<ul>"
               "<li><b>Doom-Style Segment Wall & Floor Rendering:</b> Visualizes segment walls, custom floors, ceilings, and gantry walkways.</li>"
               "<li><b>Multi-Overlay Engine Architecture:</b> Accurate overlay placement for doorways, CSG punch-outs, and corridors.</li>"
               "<li><b>Floor-by-Floor Navigation:</b> Full layer switching (0..20) via toolbar, shortcuts (PageUp/PageDown), and mouse wheel.</li>"
               "<li><b>Entity Browser & Inspector:</b> Inspect, filter, search, and edit placed map entities.</li>"
               "<li><b>PVS Visibility Zones & Portals:</b> Complete room topology, portal leak detection, and culling visualization.</li>"
               "<li><b>Memory Footprint Analyzer:</b> Measures memory weight in MB for 3D meshes, textures, and audio buffers with 32-bit limit warnings.</li>"
               "</ul>"
               "<p>Built with <b>Qt 5.15.2 (MinGW 32-bit)</b>.</p>")
            .arg(VersionInfo::AppName)
            .arg(VersionInfo::Version)
            .arg(VersionInfo::AppSubtitle)
            .arg(VersionInfo::Developer)
            .arg(VersionInfo::Studio));
    });

    // -------------------------------------------------------------
    // Main Toolbar
    // -------------------------------------------------------------
    QToolBar* mainBar = addToolBar(QStringLiteral("Main Controls"));
    mainBar->setObjectName("MainToolBar");
    mainBar->setMovable(false);
    mainBar->setIconSize(QSize(18, 18));

    m_actReload->setIcon(makeReloadIcon());
    mainBar->addAction(m_actReload);
    QToolButton* btnReload = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actReload));
    if (btnReload) {
        btnReload->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    mainBar->addSeparator();

    // Floor Navigation Controls
    m_actFloorDown = mainBar->addAction(QStringLiteral("▼"), m_canvas, &MapCanvas::floorDown);
    m_actFloorDown->setShortcuts({QKeySequence(Qt::Key_PageDown), QKeySequence(Qt::Key_Minus), QKeySequence(Qt::Key_Underscore)});
    QToolButton* btnDown = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actFloorDown));
    if (btnDown) {
        btnDown->setFixedWidth(26);
    }

    m_floorCombo = new QComboBox(this);
    m_floorCombo->setMinimumWidth(140);
    m_floorCombo->setMaximumWidth(240);
    m_floorCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    if (m_floorCombo->view()) {
        m_floorCombo->view()->setMinimumWidth(240);
    }
    mainBar->addWidget(m_floorCombo);
    connect(m_floorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFloorComboChanged);

    m_actFloorUp = mainBar->addAction(QStringLiteral("▲"), m_canvas, &MapCanvas::floorUp);
    m_actFloorUp->setShortcuts({QKeySequence(Qt::Key_PageUp), QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_Equal)});
    QToolButton* btnUp = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actFloorUp));
    if (btnUp) {
        btnUp->setFixedWidth(26);
    }
    mainBar->addSeparator();

    // View Toggles (Grouped together)
    mainBar->addAction(m_actGhostLayer);
    QToolButton* btnGhost = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actGhostLayer));
    if (btnGhost) {
        btnGhost->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    m_actEntities->setIcon(makeEntityToggleIcon());
    mainBar->addAction(m_actEntities);
    QToolButton* btnEntities = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actEntities));
    if (btnEntities) {
        btnEntities->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    mainBar->addAction(m_actShowPortals);
    QToolButton* btnPortals = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actShowPortals));
    if (btnPortals) {
        btnPortals->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    mainBar->addAction(m_actColorAllZones);
    QToolButton* btnColorZones = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actColorAllZones));
    if (btnColorZones) {
        btnColorZones->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    m_actZoomFit->setIcon(makeZoomFitIcon());
    mainBar->addAction(m_actZoomFit);
    QToolButton* btnFit = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actZoomFit));
    if (btnFit) {
        btnFit->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    mainBar->addSeparator();

    // Analysis Tools
    m_actLaunchMem = mainBar->addAction(makeMemoryIcon(), QString(), this, &MainWindow::onOpenMemoryAnalyzer);
    QToolButton* btnMem = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actLaunchMem));
    if (btnMem) {
        btnMem->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }

    m_actLaunchLeaks = mainBar->addAction(QString(), this, &MainWindow::onOpenPortalLeakDetector);
    QToolButton* btnLeaks = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actLaunchLeaks));
    if (btnLeaks) {
        btnLeaks->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }

    m_actLaunchTracer = mainBar->addAction(makeTracerIcon(), QString(), this, [this]() { onOpenZoneVisibilityTracer(); });
    QToolButton* btnTracer = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actLaunchTracer));
    if (btnTracer) {
        btnTracer->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    mainBar->addSeparator();

    // Visibility Zones & Portals Panel Toggle Button (in main toolbar)
    m_actToggleVisZone = mainBar->addAction(QString(), this, [this]() {});
    m_actToggleVisZone->setCheckable(true);
    m_actToggleVisZone->setChecked(true);
    QToolButton* btnVis = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actToggleVisZone));
    if (btnVis) {
        btnVis->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    connect(m_actToggleVisZone, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            onOpenVisZoneDock();
        } else {
            m_visZoneDock->hide();
        }
    });
    connect(m_visZoneDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_actToggleVisZone->setChecked(visible);
        if (visible) {
            if (m_actColorAllZones && !m_actColorAllZones->isChecked()) {
                m_actColorAllZones->setChecked(true);
            }
        }
    });

    retranslateUi();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
    // Menus
    if (m_fileMenu) m_fileMenu->setTitle(tr("&File"));
    if (m_viewMenu) m_viewMenu->setTitle(tr("&View"));
    if (m_portalsMenu) m_portalsMenu->setTitle(tr("&Portals"));
    if (m_toolsMenu) m_toolsMenu->setTitle(tr("&Tools"));
    if (m_languageMenu) m_languageMenu->setTitle(tr("&Language"));
    if (m_helpMenu) m_helpMenu->setTitle(tr("&Help"));
    if (m_recentMapsMenu) m_recentMapsMenu->setTitle(tr("&Recent Maps"));

    // File Actions
    if (m_actOpen) m_actOpen->setText(tr("&Open Map (.FPM)..."));
    if (m_actSave) m_actSave->setText(tr("&Save Map"));
    if (m_actSaveAs) m_actSaveAs->setText(tr("Save Map &As..."));
    if (m_actReload) {
        m_actReload->setText(tr("&Reload Map"));
        m_actReload->setToolTip(tr("Reload map from disk (F5)"));
    }
    if (m_actConfigEngine) m_actConfigEngine->setText(tr("&Configure FPS Creator Path..."));
    if (m_actExit) m_actExit->setText(tr("E&xit"));

    // View Actions
    if (m_actZoomIn) m_actZoomIn->setText(tr("Zoom &In"));
    if (m_actZoomOut) m_actZoomOut->setText(tr("Zoom &Out"));
    if (m_actZoomReset) m_actZoomReset->setText(tr("Reset Zoom (100%)"));
    if (m_actZoomFit) {
        m_actZoomFit->setText(tr("Fit View"));
        m_actZoomFit->setToolTip(tr("Fit whole map in view (Home)"));
    }
    if (m_actWallTex) m_actWallTex->setText(tr("&Wall Textures"));
    if (m_actFloorTex) m_actFloorTex->setText(tr("&Floor Textures"));
    if (m_actGrid) m_actGrid->setText(tr("&Grid Lines"));
    if (m_actEntities) {
        m_actEntities->setText(tr("&Entities"));
        m_actEntities->setToolTip(tr("Toggle entity rendering on map (E)"));
    }
    if (m_actLights) m_actLights->setText(tr("Light &Halos"));
    if (m_actZones) m_actZones->setText(tr("Trigger &Zones"));
    if (m_actWaypoints) m_actWaypoints->setText(tr("&Waypoints"));
    if (m_actGhostLayer) {
        m_actGhostLayer->setText(tr("&Ghost Lower Floor"));
        m_actGhostLayer->setToolTip(tr("Show Ghost Lower Floor (toggle semi-transparent rendering of the floor below)"));
    }
    if (m_actShowPortals) {
        m_actShowPortals->setText(tr("&Portals"));
        m_actShowPortals->setToolTip(tr("Toggle Portals & VisZones display"));
    }
    if (m_actColorAllZones) {
        m_actColorAllZones->setText(tr("Vis Zones"));
        m_actColorAllZones->setToolTip(tr("Color all visibility zones with unique colors overlay (Ctrl+Shift+C)"));
    }

    // Portals & Tools Actions
    if (m_actPvsPanel) m_actPvsPanel->setText(tr("👁 &Visibility Zones & Portals Panel (PVS)..."));
    if (m_actResetView) m_actResetView->setText(tr("🔄 &Show All Zones (Normal View)"));
    if (m_actLeakDetector) m_actLeakDetector->setText(tr("&Leak Detector..."));
    if (m_actTraceVisibility) {
        m_actTraceVisibility->setText(tr("👁️ &Trace Zone Visibility (PVS)..."));
        m_actTraceVisibility->setToolTip(tr("Inspect why zones are rendered and trace visibility pathways (Ctrl+T)"));
    }
    if (m_actMemoryAnalyzer) m_actMemoryAnalyzer->setText(tr("&Memory Analyzer..."));
    if (m_actSegmentEditor) m_actSegmentEditor->setText(tr("🧱 &Segment Inspector && Editor..."));
    if (m_actDichotomyDelete) {
        m_actDichotomyDelete->setText(tr("✂️ &Dichotomy Tool: Delete Zone && Entities..."));
        m_actDichotomyDelete->setToolTip(tr("Delete active visibility zone room, all its tiles, ceiling slabs, and entities (Shift+Del)"));
    }
    if (m_actAbout) m_actAbout->setText(tr("&About %1...").arg(VersionInfo::AppName));

    // Toolbar Buttons
    if (m_actFloorDown) m_actFloorDown->setToolTip(tr("Go one floor down (PageDown / -)"));
    if (m_actFloorUp) m_actFloorUp->setToolTip(tr("Go one floor up (PageUp / +)"));
    if (m_actLaunchMem) {
        m_actLaunchMem->setText(tr("Memory"));
        m_actLaunchMem->setToolTip(tr("Measure level RAM weight in Megabytes and inspect memory budget (Ctrl+M)"));
    }
    if (m_actLaunchLeaks) {
        m_actLaunchLeaks->setText(tr("🔍 Leaks"));
        m_actLaunchLeaks->setToolTip(tr("Scan compiled universe.dbu and map geometry for occlusion leaks"));
    }
    if (m_actLaunchTracer) {
        m_actLaunchTracer->setText(tr("👁️ Trace PVS"));
        m_actLaunchTracer->setToolTip(tr("Trace PVS Visibility Pathways & Leak Chains (Ctrl+T)"));
    }
    if (m_actToggleVisZone) {
        m_actToggleVisZone->setText(tr("👁 VisZones"));
        m_actToggleVisZone->setToolTip(tr("Toggle Visibility Zones & Portals (PVS) right dock panel"));
    }

    // Language action checks
    if (m_actLangAuto) m_actLangAuto->setText(tr("System Default"));

    auto curLang = LanguageManager::instance().currentLanguage();
    if (m_actLangAuto) m_actLangAuto->setChecked(curLang == LanguageManager::Language::Auto);
    if (m_actLangEn) m_actLangEn->setChecked(curLang == LanguageManager::Language::English);
    if (m_actLangRu) m_actLangRu->setChecked(curLang == LanguageManager::Language::Russian);

    // Dock titles
    if (m_searchDock) m_searchDock->setWindowTitle(tr("Entity Search & Palette"));
    if (m_inspectorDock) m_inspectorDock->setWindowTitle(tr("Entity Properties Inspector"));
    if (m_visZoneDock) m_visZoneDock->setWindowTitle(tr("Visibility Zones & Portals (PVS)"));

    updateWindowTitle();
    updateFloorControls();
    updateStatusBar();
}

void MainWindow::updateWindowTitle() {
    QString title = VersionInfo::fullTitle();
    if (m_currentMap) {
        QString fName = QFileInfo(m_currentMap->filePath).fileName();
        if (fName.isEmpty()) fName = m_currentMap->mapName;
        title = VersionInfo::mapTitle(fName, m_currentMap->isModified);
    }
    setWindowTitle(title);
}

bool MainWindow::maybeSave() {
    if (!m_currentMap || !m_currentMap->isModified) return true;

    auto res = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("The map '%1' has unsaved modifications.\nDo you want to save your changes?")
            .arg(QFileInfo(m_currentMap->filePath).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    if (res == QMessageBox::Save) {
        onSaveMap();
        return !m_currentMap->isModified;
    } else if (res == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        int halfH = (height() - 100) / 2;
        resizeDocks({m_inspectorDock, m_visZoneDock}, {halfH, halfH}, Qt::Vertical);
    }
}

void MainWindow::onSaveMap() {
    if (!m_currentMap) return;
    if (m_currentMap->filePath.isEmpty()) {
        onSaveMapAs();
        return;
    }
    bool ok = FPMWriter::saveMap(m_currentMap, m_currentMap->filePath, m_currentMap->password);
    if (ok) {
        updateWindowTitle();
        if (m_portalLeakDialog) {
            m_portalLeakDialog->updateDialogTitle();
        }
        statusBar()->showMessage(tr("Saved %1").arg(m_currentMap->filePath), 4000);
    } else {
        QMessageBox::critical(this, tr("Save Error"), tr("Failed to save map to:\n%1").arg(m_currentMap->filePath));
    }
}

void MainWindow::onSaveMapAs() {
    if (!m_currentMap) return;
    QString curPath = m_currentMap->filePath;
    if (curPath.isEmpty()) {
        curPath = AssetManager::instance().engineRoot() + "/Files/mapbank/" + m_currentMap->mapName + ".fpm";
    }
    QString savePath = QFileDialog::getSaveFileName(
        this,
        tr("Save FPS Creator Map As"),
        curPath,
        tr("FPS Creator Project Map (*.fpm);;All Files (*.*)")
    );
    if (savePath.isEmpty()) return;

    bool ok = FPMWriter::saveMap(m_currentMap, savePath, m_currentMap->password);
    if (ok) {
        updateWindowTitle();
        if (m_portalLeakDialog) {
            m_portalLeakDialog->updateDialogTitle();
        }
        statusBar()->showMessage(tr("Saved as %1").arg(savePath), 4000);
    } else {
        QMessageBox::critical(this, tr("Save Error"), tr("Failed to save map to:\n%1").arg(savePath));
    }
}

void MainWindow::onEntityModified(int) {
    if (!m_currentMap) return;
    m_currentMap->isModified = true;
    updateWindowTitle();
    m_canvas->update();
    m_inspectorDock->refreshValues();
    m_searchDock->rebuildTable();
    updateStatusBar();
}

void MainWindow::deleteEntity(int index) {
    if (!m_currentMap || index < 0 || index >= m_currentMap->placedEntities.size()) return;

    QString entName = m_currentMap->placedEntities[index].instanceName;
    if (entName.isEmpty() && m_currentMap->placedEntities[index].profile)
        entName = m_currentMap->placedEntities[index].profile->name;
    if (entName.isEmpty()) entName = tr("Entity #%1").arg(index);

    m_currentMap->placedEntities.erase(m_currentMap->placedEntities.begin() + index);
    m_currentMap->isModified = true;

    m_canvas->selectEntity(-1);
    m_searchDock->selectEntity(-1);
    m_inspectorDock->clear();

    m_canvas->update();
    m_searchDock->rebuildTable();
    updateFloorControls();
    updateStatusBar();
    updateWindowTitle();

    m_statusCoords->setText(tr("Deleted %1").arg(entName));
}

void MainWindow::deleteVisZone(int zoneId) {
    if (!m_currentMap || !m_visZoneManager) {
        return;
    }

    if (zoneId < 0) {
        if (m_canvas) zoneId = m_canvas->activeVisZone();
        if (zoneId < 0 && m_visZoneDock) zoneId = m_visZoneDock->activeZoneId();
    }

    if (zoneId < 0) {
        m_statusCoords->setText(tr("✂️ Dichotomy Tool: Select a zone on canvas or in VisZone panel first"));
        return;
    }

    const VisZone* zone = m_visZoneManager->getZone(zoneId);
    if (!zone) {
        m_statusCoords->setText(tr("✂️ Dichotomy Tool: Zone %1 not found").arg(zoneId + 1));
        return;
    }

    int removedTiles = 0;
    int removedEntities = 0;
    int removedWaypoints = 0;

    int rows = m_currentMap->header.maxY + 1;
    int cols = m_currentMap->header.maxX + 1;
    int layerMax = m_currentMap->header.layerMax;

    // 1. Gather all tiles to delete across all floors of the zone
    // Also include ceiling slabs on maxFloor + 1 if no upper room exists there
    std::set<std::tuple<int, int, int>> tilesToDelete; // (layer, x, y)

    for (const auto& kv : zone->floorTiles) {
        int l = kv.first;
        for (const QPoint& pt : kv.second) {
            tilesToDelete.insert(std::make_tuple(l, pt.x(), pt.y()));
        }
    }

    // Overhead ceiling slabs:
    // If maxFloor + 1 <= layerMax, check if there are ceiling slabs (gridGround == 2) directly above
    int ceilingLayer = zone->maxFloor + 1;
    if (ceilingLayer <= layerMax) {
        for (const QPoint& pt : zone->tiles) {
            int x = pt.x();
            int y = pt.y();
            if (y < rows && x < cols) {
                // If the tile above is not part of another valid room zone
                int upperZone = m_visZoneManager->getZoneAt(ceilingLayer, x, y);
                if (upperZone < 0) {
                    if (ceilingLayer < m_currentMap->gridGround.size() &&
                        y < m_currentMap->gridGround[ceilingLayer].size() &&
                        x < m_currentMap->gridGround[ceilingLayer][y].size() &&
                        m_currentMap->gridGround[ceilingLayer][y][x] == 2) {
                        tilesToDelete.insert(std::make_tuple(ceilingLayer, x, y));
                    }
                }
            }
        }
    }

    // 2. Clear grid data for all tilesToDelete
    for (const auto& t : tilesToDelete) {
        int l = std::get<0>(t);
        int x = std::get<1>(t);
        int y = std::get<2>(t);

        if (l >= 0 && l < m_currentMap->gridBlocks.size() &&
            y >= 0 && y < m_currentMap->gridBlocks[l].size() &&
            x >= 0 && x < m_currentMap->gridBlocks[l][y].size()) {

            bool hadBlock = (m_currentMap->gridBlocks[l][y][x] > 0);
            bool hadOverlay = (l < m_currentMap->gridOverlays.size() &&
                               y < m_currentMap->gridOverlays[l].size() &&
                               x < m_currentMap->gridOverlays[l][y].size() &&
                               m_currentMap->gridOverlays[l][y][x] > 0);

            if (hadBlock || hadOverlay) {
                removedTiles++;
            }

            m_currentMap->gridBlocks[l][y][x] = 0;

            if (l < m_currentMap->gridRotation.size() && y < m_currentMap->gridRotation[l].size() && x < m_currentMap->gridRotation[l][y].size())
                m_currentMap->gridRotation[l][y][x] = 0;
            if (l < m_currentMap->gridOrientation.size() && y < m_currentMap->gridOrientation[l].size() && x < m_currentMap->gridOrientation[l][y].size())
                m_currentMap->gridOrientation[l][y][x] = 0;
            if (l < m_currentMap->gridTileType.size() && y < m_currentMap->gridTileType[l].size() && x < m_currentMap->gridTileType[l][y].size())
                m_currentMap->gridTileType[l][y][x] = 0;
            if (l < m_currentMap->gridGround.size() && y < m_currentMap->gridGround[l].size() && x < m_currentMap->gridGround[l][y].size())
                m_currentMap->gridGround[l][y][x] = 0;
            if (l < m_currentMap->gridSymbol.size() && y < m_currentMap->gridSymbol[l].size() && x < m_currentMap->gridSymbol[l][y].size())
                m_currentMap->gridSymbol[l][y][x] = 0;

            if (l < m_currentMap->gridOverlays.size() && y < m_currentMap->gridOverlays[l].size() && x < m_currentMap->gridOverlays[l][y].size())
                m_currentMap->gridOverlays[l][y][x] = 0;
            if (l < m_currentMap->gridOverlayRotation.size() && y < m_currentMap->gridOverlayRotation[l].size() && x < m_currentMap->gridOverlayRotation[l][y].size())
                m_currentMap->gridOverlayRotation[l][y][x] = 0;
            if (l < m_currentMap->gridTileOverlays.size() && y < m_currentMap->gridTileOverlays[l].size() && x < m_currentMap->gridTileOverlays[l][y].size())
                m_currentMap->gridTileOverlays[l][y][x].clear();
        }
    }

    // 3. Collect entities to delete
    std::vector<int> entIndicesToDelete;
    for (int idx : zone->entityIndices) {
        if (idx >= 0 && idx < m_currentMap->placedEntities.size()) {
            entIndicesToDelete.push_back(idx);
        }
    }
    for (int i = 0; i < m_currentMap->placedEntities.size(); ++i) {
        const auto& e = m_currentMap->placedEntities[i];
        int l = e.floorLayer;
        int tx = static_cast<int>(e.x / 100.0f);
        int ty = static_cast<int>(std::abs(e.z) / 100.0f);

        if (tilesToDelete.count(std::make_tuple(l, tx, ty)) > 0 ||
            m_visZoneManager->getZoneAt(l, tx, ty) == zoneId) {
            entIndicesToDelete.push_back(i);
        }
    }
    std::sort(entIndicesToDelete.begin(), entIndicesToDelete.end(), std::greater<int>());
    entIndicesToDelete.erase(std::unique(entIndicesToDelete.begin(), entIndicesToDelete.end()), entIndicesToDelete.end());

    removedEntities = static_cast<int>(entIndicesToDelete.size());
    for (int idx : entIndicesToDelete) {
        m_currentMap->placedEntities.erase(m_currentMap->placedEntities.begin() + idx);
    }

    // 4. Waypoints inside zone
    for (int i = m_currentMap->waypoints.size() - 1; i >= 0; --i) {
        const auto& wp = m_currentMap->waypoints[i];
        int l = static_cast<int>(std::round(wp.y / 100.0f));
        int tx = static_cast<int>(std::floor(wp.x / 100.0f));
        int ty = static_cast<int>(std::floor(std::abs(wp.z) / 100.0f));
        if (tilesToDelete.count(std::make_tuple(l, tx, ty)) > 0 ||
            m_visZoneManager->getZoneAt(l, tx, ty) == zoneId) {
            m_currentMap->waypoints.erase(m_currentMap->waypoints.begin() + i);
            removedWaypoints++;
        }
    }

    // 5. Mark modified, rebuild VisZoneManager & UI
    m_currentMap->isModified = true;
    m_visZoneManager->buildFromMap(m_currentMap);

    // Reset selection
    if (m_canvas) {
        m_canvas->setActiveVisZone(-1);
        m_canvas->selectEntity(-1);
        m_canvas->clearHighlight();
        m_canvas->setVisZoneManager(m_visZoneManager);
        m_canvas->update();
    }
    if (m_visZoneDock) {
        m_visZoneDock->setVisZoneManager(m_visZoneManager);
        m_visZoneDock->onExternalZoneSelected(-1);
    }
    if (m_searchDock) {
        m_searchDock->selectEntity(-1);
        m_searchDock->rebuildTable();
    }
    if (m_inspectorDock) {
        m_inspectorDock->clear();
    }

    if (m_portalLeakDialog && m_portalLeakDialog->isVisible()) {
        m_portalLeakDialog->runAnalysis();
    }

    updateFloorControls();
    updateStatusBar();
    updateWindowTitle();

    QString msg = tr("✂️ Dichotomy Tool: Room Zone %1 deleted (removed %2 tiles, %3 entities).")
                  .arg(zoneId + 1).arg(removedTiles).arg(removedEntities);
    m_statusCoords->setText(msg);
    statusBar()->showMessage(msg, 5000);
}

static QString normalizeMapPath(const QString& path) {
    if (path.isEmpty()) return QString();
    QFileInfo fi(path);
    QString canonical = fi.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return QDir::cleanPath(canonical);
    }
    return QDir::cleanPath(fi.absoluteFilePath());
}

static bool areSameMapFiles(const QString& p1, const QString& p2) {
    if (p1.isEmpty() || p2.isEmpty()) return false;
    QString n1 = normalizeMapPath(p1);
    QString n2 = normalizeMapPath(p2);
#if defined(Q_OS_WIN)
    return n1.compare(n2, Qt::CaseInsensitive) == 0;
#else
    return n1 == n2;
#endif
}

void MainWindow::populateRecentMapsMenu() {
    m_recentMapsMenu->clear();
    QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
    QStringList recent = settings.value(QStringLiteral("recentMaps")).toStringList();

    // Filter valid existing files and eliminate all duplicates (case-insensitive on Windows)
    QStringList valid;
    for (const QString& f : recent) {
        QString norm = normalizeMapPath(f);
        if (norm.isEmpty() || !QFile::exists(norm)) continue;

        bool alreadyPresent = false;
        for (const QString& v : valid) {
            if (areSameMapFiles(norm, v)) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent) {
            valid.append(norm);
        }
    }
    settings.setValue(QStringLiteral("recentMaps"), valid);

    if (valid.isEmpty()) {
        QAction* emptyAct = m_recentMapsMenu->addAction(tr("No Recent Maps"));
        emptyAct->setEnabled(false);
        return;
    }

    for (int i = 0; i < valid.size() && i < 15; ++i) {
        const QString& fPath = valid[i];
        QString name = QFileInfo(fPath).fileName();
        QString text = QString("&%1 %2").arg(i + 1).arg(name);
        QAction* act = m_recentMapsMenu->addAction(text, this, [this, fPath]() {
            if (maybeSave()) {
                bool isSameFile = (m_currentMap && areSameMapFiles(m_currentMap->filePath, fPath));
                loadMapFile(fPath, isSameFile);
            }
        });
        act->setToolTip(fPath);
        act->setStatusTip(fPath);
    }

    m_recentMapsMenu->addSeparator();
    m_recentMapsMenu->addAction(tr("Clear Recent Maps"), this, [this]() {
        QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
        settings.remove(QStringLiteral("recentMaps"));
        populateRecentMapsMenu();
    });
}

void MainWindow::loadMapFile(const QString& filePath, bool preserveView) {
    QElapsedTimer timer;
    timer.start();

    QProgressDialog progress(
        tr("Loading %1...").arg(QFileInfo(filePath).fileName()),
        QString(), 0, 100, this
    );
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    progress.setStyleSheet(
        "QProgressDialog { background-color: #1e1e24; color: #e0e0e0; border: 1px solid #3d3d45; font-family: 'Segoe UI'; min-width: 340px; }"
        "QLabel { color: #e0e0e0; font-size: 12px; margin-bottom: 8px; }"
        "QProgressBar { background-color: #151518; border: 1px solid #33333d; border-radius: 4px; height: 18px; text-align: center; color: #ffffff; font-size: 11px; }"
        "QProgressBar::chunk { background-color: #2a82da; border-radius: 3px; }"
    );
    progress.show();
    QCoreApplication::processEvents();

    auto progressCb = [&](int pct, const QString& msg) {
        progress.setValue(pct / 2); // 0..50%
        progress.setLabelText(msg);
        QCoreApplication::processEvents();
    };

    auto map = FPMReader::loadMap(filePath, "mypassword", progressCb);
    if (!map) {
        progress.close();
        QMessageBox::critical(this, tr("Error"), tr("Failed to load FPM map file:\n%1").arg(filePath));
        return;
    }

    m_currentMap = map;

    // Start memory footprint calculation asynchronously in background worker
    m_memoryReportValid = false;
    startAsyncMemoryAnalysis(m_currentMap);

    progress.setValue(70);
    progress.setLabelText(tr("Building visibility zones & portals..."));
    QCoreApplication::processEvents();
    m_canvas->setMap(m_currentMap, preserveView);

    progress.setValue(85);
    progress.setLabelText(tr("Populating entity list..."));
    QCoreApplication::processEvents();
    m_searchDock->setCurrentFloor(m_canvas->currentFloor());
    m_searchDock->setMap(m_currentMap);
    if (!preserveView) {
        m_inspectorDock->clear();
    } else if (m_canvas->selectedEntityIndex() >= 0 && m_canvas->selectedEntityIndex() < m_currentMap->placedEntities.size()) {
        m_inspectorDock->setEntity(m_currentMap, m_canvas->selectedEntityIndex());
        m_searchDock->selectEntity(m_canvas->selectedEntityIndex());
    } else {
        m_inspectorDock->clear();
    }

    if (m_visZoneDock) {
        m_visZoneDock->setMap(m_currentMap);
    }

    if (m_portalLeakDialog && m_portalLeakDialog->isVisible()) {
        m_portalLeakDialog->setMap(m_currentMap);
    }
    if (m_memoryAnalyzerDialog && m_memoryAnalyzerDialog->isVisible()) {
        m_memoryAnalyzerDialog->setMap(m_currentMap, nullptr);
    }

    progress.setValue(95);
    progress.setLabelText(tr("Rendering map canvas..."));
    QCoreApplication::processEvents();
    m_canvas->repaint();

    progress.setValue(100);
    progress.close();

    // Save to Recent Maps list in QSettings with robust deduplication
    QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
    QStringList recent = settings.value(QStringLiteral("recentMaps")).toStringList();
    QString normCurrent = normalizeMapPath(filePath);

    QStringList updatedRecent;
    if (!normCurrent.isEmpty()) {
        updatedRecent.append(normCurrent);
    }
    for (const QString& f : recent) {
        QString norm = normalizeMapPath(f);
        if (!norm.isEmpty() && !areSameMapFiles(norm, normCurrent) && QFile::exists(norm)) {
            bool alreadyIn = false;
            for (const QString& u : updatedRecent) {
                if (areSameMapFiles(u, norm)) {
                    alreadyIn = true;
                    break;
                }
            }
            if (!alreadyIn) {
                updatedRecent.append(norm);
            }
        }
    }
    while (updatedRecent.size() > 15) {
        updatedRecent.removeLast();
    }
    settings.setValue(QStringLiteral("recentMaps"), updatedRecent);
    populateRecentMapsMenu();

    updateWindowTitle();
    updateFloorControls();
    updateStatusBar();
    m_statusMapName->setText(tr("Loaded \"%1\" (%2 entities, %3 segments) in %4 ms")
        .arg(map->mapName)
        .arg(map->placedEntities.size())
        .arg(map->segmentsBank.size())
        .arg(timer.elapsed()));
}

void MainWindow::onOpenRecentMap(const QString& filePath) {
    if (!filePath.isEmpty() && maybeSave()) {
        bool isSameFile = (m_currentMap && areSameMapFiles(m_currentMap->filePath, filePath));
        loadMapFile(filePath, isSameFile);
    }
}

void MainWindow::onOpenMap() {
    if (!maybeSave()) return;
    QString startDir = AssetManager::instance().engineRoot() + "/Files/mapbank";
    QString file = QFileDialog::getOpenFileName(this, tr("Open FPS Creator Map"), startDir, tr("FPS Creator Project Map (*.fpm);;All Files (*.*)"));
    if (!file.isEmpty()) {
        bool isSameFile = (m_currentMap && areSameMapFiles(m_currentMap->filePath, file));
        loadMapFile(file, isSameFile);
    }
}

void MainWindow::onReloadMap() {
    if (m_currentMap && !m_currentMap->filePath.isEmpty()) {
        if (!maybeSave()) return;
        loadMapFile(m_currentMap->filePath, true);
    }
}

void MainWindow::onConfigureEnginePath() {
    QString current = AssetManager::instance().engineRoot();
    QString chosen = QFileDialog::getExistingDirectory(this, tr("Select FPS Creator Installation Directory"), current);
    if (!chosen.isEmpty()) {
        AssetManager::instance().setEngineRoot(chosen);
        populateRecentMapsMenu();
        if (m_currentMap) {
            onReloadMap();
        }
    }
}

void MainWindow::onOpenMemoryAnalyzer() {
    if (!m_currentMap) {
        QMessageBox::information(this, tr("No Map Loaded"), tr("Please open an FPS Creator map (.FPM) first."));
        return;
    }

    if (!m_memoryAnalyzerDialog) {
        m_memoryAnalyzerDialog = new MemoryAnalyzerDialog(
            m_currentMap,
            m_memoryReportValid ? &m_cachedMemoryReport : nullptr,
            this
        );
        m_memoryAnalyzerDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_memoryAnalyzerDialog->show();
    } else {
        if (m_memoryReportValid) {
            m_memoryAnalyzerDialog->setReport(m_cachedMemoryReport);
        } else {
            m_memoryAnalyzerDialog->setMap(m_currentMap, nullptr);
        }
        m_memoryAnalyzerDialog->raise();
        m_memoryAnalyzerDialog->activateWindow();
        m_memoryAnalyzerDialog->show();
    }
}

void MainWindow::updateFloorControls() {
    if (!m_floorCombo) return;
    if (!m_currentMap) return;

    m_isUpdatingFloorUI = true;
    m_floorCombo->clear();

    int layerMax = m_currentMap->header.layerMax;
    if (m_floorSpin) m_floorSpin->setRange(0, layerMax);

    // Count entities and segments per layer
    QVector<int> entCounts(layerMax + 1, 0);
    for (const auto& ent : m_currentMap->placedEntities) {
        if (ent.floorLayer >= 0 && ent.floorLayer <= layerMax) {
            entCounts[ent.floorLayer]++;
        }
    }

    QVector<int> segCounts(layerMax + 1, 0);
    int rows = m_currentMap->header.maxY + 1;
    int cols = m_currentMap->header.maxX + 1;
    for (int l = 0; l <= layerMax && l < m_currentMap->gridBlocks.size(); ++l) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (m_currentMap->gridBlocks[l][y][x] > 0) segCounts[l]++;
            }
        }
    }

    for (int l = 0; l <= layerMax; ++l) {
        QString label = tr("Floor %1 (Y: %2)").arg(l).arg(l * 100);
        if (entCounts[l] > 0 || segCounts[l] > 0) {
            label += tr(" [%1 ent, %2 seg]").arg(entCounts[l]).arg(segCounts[l]);
        }
        m_floorCombo->addItem(label, l);
    }

    int active = qBound(0, m_canvas->currentFloor(), layerMax);
    m_floorCombo->setCurrentIndex(active);
    if (m_floorSpin) m_floorSpin->setValue(active);
    m_isUpdatingFloorUI = false;
}

void MainWindow::onFloorComboChanged(int index) {
    if (m_isUpdatingFloorUI) return;
    if (index >= 0) {
        m_canvas->setFloor(index);
    }
}

void MainWindow::onFloorSpinChanged(int value) {
    if (m_isUpdatingFloorUI) return;
    m_canvas->setFloor(value);
}

void MainWindow::onFloorSliderChanged(int value) {
    if (m_isUpdatingFloorUI) return;
    m_canvas->setFloor(value);
}

void MainWindow::onCanvasFloorChanged(int floor) {
    m_isUpdatingFloorUI = true;
    m_floorCombo->setCurrentIndex(floor);
    if (m_floorSpin) m_floorSpin->setValue(floor);
    m_isUpdatingFloorUI = false;

    m_searchDock->setCurrentFloor(floor);

    if (m_visZoneDock) {
        m_visZoneDock->onFloorChanged(floor);
    }
    updateStatusBar();
}

void MainWindow::onEntitySelected(int index) {
    if (m_canvas->selectedEntityIndex() != index) {
        m_canvas->selectEntity(index);
    }
    m_searchDock->selectEntity(index);
    m_inspectorDock->setEntity(m_currentMap, index);
}

void MainWindow::onFocusEntityRequested(int index) {
    m_canvas->focusOnEntity(index);
    m_searchDock->selectEntity(index);
    m_inspectorDock->setEntity(m_currentMap, index);
}

void MainWindow::onHoverInfoChanged(const QString& info) {
    m_statusCoords->setText(info);
}

void MainWindow::onZoomChanged(float) {
    updateStatusBar();
}

void MainWindow::updateStatusBar() {
    if (!m_statusMapName || !m_statusFloor || !m_statusMemory) return;

    if (!m_currentMap) {
        m_statusMapName->setText(tr("No map loaded"));
        m_statusFloor->setText(tr("Floor: 0"));
        m_statusMemory->setText(tr("RAM: 0 MB"));
        return;
    }

    m_statusMapName->setText(tr("Map: %1 (%2 entities, %3 segments)")
        .arg(m_currentMap->mapName)
        .arg(m_currentMap->placedEntities.size())
        .arg(m_currentMap->segmentsBank.size()));

    m_statusFloor->setText(tr("Floor: %1 / %2 (Height: %3 units)")
        .arg(m_canvas->currentFloor())
        .arg(m_currentMap->header.layerMax)
        .arg(m_canvas->currentFloor() * 100));

    if (m_memoryReportValid) {
        m_statusMemory->setText(tr("Level RAM: %1 MB (%2%)")
            .arg(m_cachedMemoryReport.totalEstimatedRamBytes / (1024.0 * 1024.0), 0, 'f', 1)
            .arg(m_cachedMemoryReport.engineLimitPercent, 0, 'f', 1));
    } else if (m_isMemoryAnalyzing) {
        m_statusMemory->setText(tr("Level RAM: calculating..."));
    } else {
        m_statusMemory->setText(tr("Level RAM: --"));
    }
}

void MainWindow::startAsyncMemoryAnalysis(std::shared_ptr<FPSCMap> map) {
    if (!map) return;

    m_isMemoryAnalyzing = true;
    m_memoryReportValid = false;
    updateStatusBar();

    uint64_t token = ++m_memoryAnalysisToken;
    auto* watcher = new QFutureWatcher<MemoryReport>(this);

    connect(watcher, &QFutureWatcher<MemoryReport>::finished, this, [this, watcher, token, map]() {
        if (token == m_memoryAnalysisToken && m_currentMap == map) {
            m_cachedMemoryReport = watcher->result();
            m_memoryReportValid = true;
            m_isMemoryAnalyzing = false;
            updateStatusBar();

            if (m_memoryAnalyzerDialog && m_memoryAnalyzerDialog->isVisible()) {
                m_memoryAnalyzerDialog->setReport(m_cachedMemoryReport);
            }
        }
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([map]() {
        return MemoryAnalyzer::analyze(map);
    }));
}

void MainWindow::onOpenPortalLeakDetector() {
    if (!m_currentMap) {
        QMessageBox::warning(this, tr("Error"), tr("Please open a map first."));
        return;
    }
    if (!m_portalLeakDialog) {
        m_portalLeakDialog = new PortalLeakDialog(m_currentMap, m_visZoneManager, this);
        m_portalLeakDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_portalLeakDialog, &PortalLeakDialog::cellSelected, m_canvas, &MapCanvas::highlightCell);
        connect(m_portalLeakDialog, &PortalLeakDialog::warningsUpdated, m_canvas, &MapCanvas::setLeakWarnings);
        connect(m_portalLeakDialog, &PortalLeakDialog::mapModified, this, [this]() {
            if (m_currentMap) {
                m_currentMap->isModified = true;
                updateWindowTitle();
            }
        });
        connect(m_portalLeakDialog, &QDialog::finished, m_canvas, [this]() {
            m_canvas->clearLeakWarnings();
            m_canvas->clearHighlight();
        });
        connect(m_portalLeakDialog, &PortalLeakDialog::resolveConflictRequested, this, &MainWindow::onResolveSegmentConflict);
        connect(m_portalLeakDialog, &PortalLeakDialog::editSegmentRequested, this, [this](int l, int x, int y) {
            onOpenSegmentEditor(l, x, y);
        });
        m_portalLeakDialog->show();
        m_canvas->setLeakWarnings(m_portalLeakDialog->currentWarnings());
    } else {
        m_portalLeakDialog->setVisZoneManager(m_visZoneManager);
        m_portalLeakDialog->setMap(m_currentMap);
        m_portalLeakDialog->raise();
        m_portalLeakDialog->activateWindow();
        m_portalLeakDialog->show();
        m_canvas->setLeakWarnings(m_portalLeakDialog->currentWarnings());
    }

    // Also auto-refresh canvas portals
    PortalLeakAnalyzer analyzer(m_currentMap, m_visZoneManager);
    analyzer.analyze();
    m_canvas->setPortals(analyzer.allPortals(), analyzer.allZones());
}

void MainWindow::onOpenVisZoneDock() {
    if (m_visZoneDock) {
        m_visZoneDock->show();
        m_visZoneDock->raise();
        m_visZoneDock->activateWindow();
        m_visZoneDock->setTab(0);
        if (m_actColorAllZones && !m_actColorAllZones->isChecked()) {
            m_actColorAllZones->setChecked(true);
        }
    }
}

void MainWindow::onOpenZoneVisibilityTracer(int initialZoneId) {
    if (!m_currentMap) {
        QMessageBox::warning(this, tr("Error"), tr("Please open a map first."));
        return;
    }
    int initialZone = initialZoneId;
    if (initialZone < 0 && m_canvas && m_canvas->activeVisZone() >= 0) {
        initialZone = m_canvas->activeVisZone();
    }
    if (initialZone < 0 && m_visZoneDock) {
        int az = m_visZoneDock->activeZoneId();
        if (az >= 0) initialZone = az;
    }
    if (initialZone < 0) initialZone = 0;

    if (m_visZoneDock) {
        m_visZoneDock->show();
        m_visZoneDock->raise();
        m_visZoneDock->activateWindow();
        m_visZoneDock->onExternalZoneSelected(initialZone);
        m_visZoneDock->setTab(1);
    }
}

void MainWindow::onOpenSegmentEditor(int layer, int x, int y) {
    if (!m_currentMap) {
        QMessageBox::warning(this, tr("Error"), tr("Please open a map first."));
        return;
    }

    if (layer < 0) layer = m_canvas ? m_canvas->currentFloor() : 0;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (!m_segmentEditorDialog) {
        m_segmentEditorDialog = new SegmentEditorDialog(m_currentMap, m_visZoneManager, this);
        m_segmentEditorDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_segmentEditorDialog, &SegmentEditorDialog::segmentModified, this, &MainWindow::onSegmentModified);
        m_segmentEditorDialog->inspectCell(layer, x, y);
        m_segmentEditorDialog->show();
    } else {
        m_segmentEditorDialog->setVisZoneManager(m_visZoneManager);
        m_segmentEditorDialog->setMap(m_currentMap);
        m_segmentEditorDialog->inspectCell(layer, x, y);
        m_segmentEditorDialog->raise();
        m_segmentEditorDialog->activateWindow();
        m_segmentEditorDialog->show();
    }
}

void MainWindow::onResolveSegmentConflict(int layer, int x1, int y1, int x2, int y2) {
    if (!m_currentMap) return;

    if (!m_segmentEditorDialog) {
        m_segmentEditorDialog = new SegmentEditorDialog(m_currentMap, m_visZoneManager, this);
        m_segmentEditorDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_segmentEditorDialog, &SegmentEditorDialog::segmentModified, this, &MainWindow::onSegmentModified);
        m_segmentEditorDialog->inspectConflict(layer, x1, y1, x2, y2);
        m_segmentEditorDialog->show();
    } else {
        m_segmentEditorDialog->setVisZoneManager(m_visZoneManager);
        m_segmentEditorDialog->setMap(m_currentMap);
        m_segmentEditorDialog->inspectConflict(layer, x1, y1, x2, y2);
        m_segmentEditorDialog->raise();
        m_segmentEditorDialog->activateWindow();
        m_segmentEditorDialog->show();
    }
}

void MainWindow::onSegmentModified(int layer, int x, int y) {
    Q_UNUSED(layer);
    Q_UNUSED(x);
    Q_UNUSED(y);
    if (!m_currentMap) return;

    m_currentMap->isModified = true;
    updateWindowTitle();

    if (m_visZoneManager) {
        m_visZoneManager->buildFromMap(m_currentMap);
    }

    if (m_canvas) {
        m_canvas->clearHighlight();
        m_canvas->update();
    }

    if (m_portalLeakDialog && m_portalLeakDialog->isVisible()) {
        m_portalLeakDialog->runAnalysis();
    }

    if (m_visZoneDock && m_visZoneDock->isVisible()) {
        m_visZoneDock->setVisZoneManager(m_visZoneManager);
        m_visZoneDock->refreshGraph();
    }
}
