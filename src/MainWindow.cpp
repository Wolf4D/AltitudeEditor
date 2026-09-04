#include "MainWindow.h"
#include "Version.h"
#include "FPMReader.h"
#include "FPMWriter.h"
#include "AssetManager.h"
#include "MemoryAnalyzerDialog.h"
#include "PortalLeakDialog.h"
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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    updateWindowTitle();
    resize(1360, 860);

    setWindowIcon(QIcon(":/app.png"));

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

    // Right Dock 2: Visibility Zones (PVS / Portals) - Opens in right dock
    m_visZoneDock = new VisZoneDock(this);
    m_visZoneDock->setVisZoneManager(m_visZoneManager);
    m_visZoneDock->setMinimumWidth(330);
    addDockWidget(Qt::RightDockWidgetArea, m_visZoneDock);

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
    connect(m_canvas, &MapCanvas::visZoneSelected, m_visZoneDock, &VisZoneDock::onExternalZoneSelected);

    // Status bar setup
    m_statusMapName = new QLabel(QStringLiteral("No map loaded"), this);
    m_statusFloor = new QLabel(QStringLiteral("Floor: 0"), this);
    m_statusCoords = new QLabel(QStringLiteral("Tile: (0, 0)"), this);
    m_statusMemory = new QLabel(QStringLiteral("RAM: 0 MB"), this);

    statusBar()->addWidget(m_statusMapName, 2);
    statusBar()->addWidget(m_statusFloor, 1);
    statusBar()->addWidget(m_statusCoords, 2);
    statusBar()->addPermanentWidget(m_statusMemory, 1);
}

void MainWindow::createMenusAndToolbars() {
    // -------------------------------------------------------------
    // Menu Bar
    // -------------------------------------------------------------
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* actOpen = fileMenu->addAction(QStringLiteral("&Open Map (.FPM)..."), this, &MainWindow::onOpenMap, QKeySequence::Open);
    m_actSave = fileMenu->addAction(QStringLiteral("&Save Map"), this, &MainWindow::onSaveMap, QKeySequence::Save);
    m_actSaveAs = fileMenu->addAction(QStringLiteral("Save Map &As..."), this, &MainWindow::onSaveMapAs, QKeySequence::SaveAs);
    QAction* actReload = fileMenu->addAction(QStringLiteral("&Reload Map"), this, &MainWindow::onReloadMap, QKeySequence::Refresh);

    m_recentMapsMenu = fileMenu->addMenu(QStringLiteral("&Recent Maps"));
    populateRecentMapsMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("&Configure FPS Creator Path..."), this, &MainWindow::onConfigureEnginePath);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &MainWindow::close, QKeySequence::Quit);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    QAction* actZoomIn = viewMenu->addAction(QStringLiteral("Zoom &In"), m_canvas, &MapCanvas::zoomIn, QKeySequence::ZoomIn);
    QAction* actZoomOut = viewMenu->addAction(QStringLiteral("Zoom &Out"), m_canvas, &MapCanvas::zoomOut, QKeySequence::ZoomOut);
    QAction* actZoomReset = viewMenu->addAction(QStringLiteral("Reset Zoom (100%)"), m_canvas, &MapCanvas::zoomReset, Qt::Key_0);
    QAction* actZoomFit = viewMenu->addAction(QStringLiteral("Fit View"), m_canvas, &MapCanvas::zoomFit, Qt::Key_Home);

    viewMenu->addSeparator();
    m_actWallTex = viewMenu->addAction(QStringLiteral("&Wall Textures"));
    m_actWallTex->setCheckable(true);
    m_actWallTex->setChecked(true);
    connect(m_actWallTex, &QAction::toggled, m_canvas, &MapCanvas::setShowWallTextures);

    m_actFloorTex = viewMenu->addAction(QStringLiteral("&Floor Textures"));
    m_actFloorTex->setCheckable(true);
    m_actFloorTex->setChecked(true);
    connect(m_actFloorTex, &QAction::toggled, m_canvas, &MapCanvas::setShowFloorTextures);

    m_actGrid = viewMenu->addAction(QStringLiteral("&Grid Lines"));
    m_actGrid->setCheckable(true);
    m_actGrid->setChecked(true);
    connect(m_actGrid, &QAction::toggled, m_canvas, &MapCanvas::setShowGrid);

    m_actEntities = viewMenu->addAction(QStringLiteral("&Entities"));
    m_actEntities->setCheckable(true);
    m_actEntities->setChecked(true);
    connect(m_actEntities, &QAction::toggled, m_canvas, &MapCanvas::setShowEntities);

    m_actLights = viewMenu->addAction(QStringLiteral("Light &Halos"));
    m_actLights->setCheckable(true);
    m_actLights->setChecked(true);
    connect(m_actLights, &QAction::toggled, m_canvas, &MapCanvas::setShowLights);

    m_actZones = viewMenu->addAction(QStringLiteral("Trigger &Zones"));
    m_actZones->setCheckable(true);
    m_actZones->setChecked(true);
    connect(m_actZones, &QAction::toggled, m_canvas, &MapCanvas::setShowZones);

    m_actWaypoints = viewMenu->addAction(QStringLiteral("&Waypoints"));
    m_actWaypoints->setCheckable(true);
    m_actWaypoints->setChecked(true);
    connect(m_actWaypoints, &QAction::toggled, m_canvas, &MapCanvas::setShowWaypoints);

    m_actGhostLayer = viewMenu->addAction(QStringLiteral("&Ghost Lower Floor"));
    m_actGhostLayer->setIcon(makeGhostFloorIcon());
    m_actGhostLayer->setCheckable(true);
    m_actGhostLayer->setChecked(true);
    m_actGhostLayer->setToolTip(QStringLiteral("Show Ghost Lower Floor (toggle semi-transparent rendering of the floor below)"));
    connect(m_actGhostLayer, &QAction::toggled, m_canvas, &MapCanvas::setShowGhostLayer);

    m_actShowPortals = viewMenu->addAction(QStringLiteral("&Portals"));
    m_actShowPortals->setIcon(makePortalIcon());
    m_actShowPortals->setCheckable(true);
    m_actShowPortals->setChecked(false);
    m_actShowPortals->setToolTip(QStringLiteral("Toggle Portals & VisZones display"));
    connect(m_actShowPortals, &QAction::toggled, this, [this](bool checked) {
        if (checked && m_canvas) {
            PortalLeakAnalyzer analyzer(m_currentMap);
            analyzer.analyze();
            m_canvas->setPortals(analyzer.allPortals(), analyzer.allZones());
        }
        m_canvas->setShowPortals(checked);
    });

    viewMenu->addSeparator();
    viewMenu->addAction(m_searchDock->toggleViewAction());
    viewMenu->addAction(m_inspectorDock->toggleViewAction());
    viewMenu->addAction(m_visZoneDock->toggleViewAction());

    QMenu* portalsMenu = menuBar()->addMenu(QStringLiteral("&Portals"));
    portalsMenu->addAction(QStringLiteral("👁 &Visibility Zones & Portals Panel (PVS)..."), this, [this]() {
        m_visZoneDock->show();
        m_visZoneDock->raise();
        m_visZoneDock->activateWindow();
    }, QKeySequence(Qt::CTRL + Qt::Key_P));
    portalsMenu->addAction(QStringLiteral("🔄 &Show All Zones (Normal View)"), m_visZoneDock, &VisZoneDock::resetToNormalView, QKeySequence(Qt::Key_Escape));
    portalsMenu->addSeparator();

    QAction* actColorAllZones = portalsMenu->addAction(QStringLiteral("🎨 &Color All Vis-Zones (Show Overlay)"));
    actColorAllZones->setCheckable(true);
    actColorAllZones->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
    connect(actColorAllZones, &QAction::toggled, m_canvas, &MapCanvas::setColorAllVisZones);
    connect(actColorAllZones, &QAction::toggled, m_visZoneDock, &VisZoneDock::setColorAllZones);
    connect(m_visZoneDock, &VisZoneDock::colorAllZonesToggled, actColorAllZones, &QAction::setChecked);
    viewMenu->addAction(actColorAllZones);

    portalsMenu->addSeparator();
    portalsMenu->addAction(m_actShowPortals);
    portalsMenu->addAction(QStringLiteral("&Leak Detector..."), this, &MainWindow::onOpenPortalLeakDetector);

    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));
    toolsMenu->addAction(QStringLiteral("&Memory Analyzer..."), this, &MainWindow::onOpenMemoryAnalyzer, QKeySequence(Qt::CTRL + Qt::Key_M));
    toolsMenu->addAction(QStringLiteral("&Leak Detector..."), this, &MainWindow::onOpenPortalLeakDetector);
    toolsMenu->addAction(QStringLiteral("&Visibility Zones & Portals Panel (PVS)..."), this, [this]() {
        m_visZoneDock->show();
        m_visZoneDock->raise();
        m_visZoneDock->activateWindow();
    });

    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QString("&About %1...").arg(VersionInfo::AppName), this, [this]() {
        QMessageBox::about(this, QString("About %1").arg(VersionInfo::shortTitle()),
            QStringLiteral("<h3>%1 v%2 - %3</h3>"
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

    mainBar->addAction(actReload);
    mainBar->addSeparator();

    // Floor Navigation Controls
    m_actFloorDown = mainBar->addAction(QStringLiteral("▼"), m_canvas, &MapCanvas::floorDown);
    m_actFloorDown->setShortcuts({QKeySequence(Qt::Key_PageDown), QKeySequence(Qt::Key_Minus), QKeySequence(Qt::Key_Underscore)});
    m_actFloorDown->setToolTip(QStringLiteral("Go one floor down (PageDown / -)"));

    m_floorCombo = new QComboBox(this);
    m_floorCombo->setMinimumWidth(230);
    m_floorCombo->setMaximumWidth(280);
    mainBar->addWidget(m_floorCombo);
    connect(m_floorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFloorComboChanged);

    m_actFloorUp = mainBar->addAction(QStringLiteral("▲"), m_canvas, &MapCanvas::floorUp);
    m_actFloorUp->setShortcuts({QKeySequence(Qt::Key_PageUp), QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_Equal)});
    m_actFloorUp->setToolTip(QStringLiteral("Go one floor up (PageUp / +)"));
    mainBar->addSeparator();

    // View Toggles (Grouped together)
    mainBar->addAction(m_actGhostLayer);
    QToolButton* btnGhost = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actGhostLayer));
    if (btnGhost) {
        btnGhost->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    mainBar->addAction(m_actEntities);
    mainBar->addAction(m_actShowPortals);
    QToolButton* btnPortals = qobject_cast<QToolButton*>(mainBar->widgetForAction(m_actShowPortals));
    if (btnPortals) {
        btnPortals->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    mainBar->addSeparator();

    // Fit in View
    mainBar->addAction(actZoomFit);
    mainBar->addSeparator();

    // Analysis Tools
    QAction* actLaunchMem = mainBar->addAction(makeMemoryIcon(), QStringLiteral("Memory"), this, &MainWindow::onOpenMemoryAnalyzer);
    actLaunchMem->setToolTip(QStringLiteral("Measure level RAM weight in Megabytes and inspect memory budget"));
    QToolButton* btnMem = qobject_cast<QToolButton*>(mainBar->widgetForAction(actLaunchMem));
    if (btnMem) {
        btnMem->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }

    QAction* actLaunchLeaks = mainBar->addAction(QStringLiteral("🔍 Leak Detector"), this, &MainWindow::onOpenPortalLeakDetector);
    actLaunchLeaks->setToolTip(QStringLiteral("Scan compiled universe.dbu and map geometry for occlusion leaks"));

    // Expanding spacer to push VisZones button to the far right
    QWidget* rightSpacer = new QWidget(this);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mainBar->addWidget(rightSpacer);

    // Visibility Zones Toggle Button (Pinned to the right, directly above the right dock!)
    QAction* actToggleVisZone = mainBar->addAction(QStringLiteral("👁 VisZones"), this, [this]() {});
    actToggleVisZone->setCheckable(true);
    actToggleVisZone->setChecked(true);
    actToggleVisZone->setToolTip(QStringLiteral("Toggle Visibility Zones & Portals (PVS) right dock panel"));
    connect(actToggleVisZone, &QAction::toggled, this, [this](bool checked) {
        m_visZoneDock->setVisible(checked);
        if (checked) {
            m_visZoneDock->raise();
        }
    });
    connect(m_visZoneDock, &QDockWidget::visibilityChanged, actToggleVisZone, &QAction::setChecked);
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
        QStringLiteral("Unsaved Changes"),
        QString("The map '%1' has unsaved modifications.\nDo you want to save your changes?")
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
        statusBar()->showMessage(QString("Saved %1").arg(m_currentMap->filePath), 4000);
    } else {
        QMessageBox::critical(this, QStringLiteral("Save Error"), QString("Failed to save map to:\n%1").arg(m_currentMap->filePath));
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
        QStringLiteral("Save FPS Creator Map As"),
        curPath,
        QStringLiteral("FPS Creator Project Map (*.fpm);;All Files (*.*)")
    );
    if (savePath.isEmpty()) return;

    bool ok = FPMWriter::saveMap(m_currentMap, savePath, m_currentMap->password);
    if (ok) {
        updateWindowTitle();
        statusBar()->showMessage(QString("Saved as %1").arg(savePath), 4000);
    } else {
        QMessageBox::critical(this, QStringLiteral("Save Error"), QString("Failed to save map to:\n%1").arg(savePath));
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
    if (entName.isEmpty()) entName = QString("Entity #%1").arg(index);

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

    statusBar()->showMessage(QString("Deleted %1").arg(entName), 4000);
}

void MainWindow::populateRecentMapsMenu() {
    m_recentMapsMenu->clear();
    QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
    QStringList recent = settings.value(QStringLiteral("recentMaps")).toStringList();

    // Filter valid existing files
    QStringList valid;
    for (const QString& f : recent) {
        if (QFile::exists(f) && !valid.contains(f)) {
            valid.append(f);
        }
    }
    settings.setValue(QStringLiteral("recentMaps"), valid);

    if (valid.isEmpty()) {
        QAction* emptyAct = m_recentMapsMenu->addAction(QStringLiteral("No Recent Maps"));
        emptyAct->setEnabled(false);
        return;
    }

    for (int i = 0; i < valid.size() && i < 15; ++i) {
        const QString& fPath = valid[i];
        QString name = QFileInfo(fPath).fileName();
        QString text = QString("&%1 %2").arg(i + 1).arg(name);
        QAction* act = m_recentMapsMenu->addAction(text, this, [this, fPath]() {
            if (maybeSave()) {
                loadMapFile(fPath);
            }
        });
        act->setToolTip(fPath);
        act->setStatusTip(fPath);
    }

    m_recentMapsMenu->addSeparator();
    m_recentMapsMenu->addAction(QStringLiteral("Clear Recent Maps"), this, [this]() {
        QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
        settings.remove(QStringLiteral("recentMaps"));
        populateRecentMapsMenu();
    });
}

void MainWindow::loadMapFile(const QString& filePath) {
    QElapsedTimer timer;
    timer.start();

    QProgressDialog progress(
        QString("Loading %1...").arg(QFileInfo(filePath).fileName()),
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
        QMessageBox::critical(this, QStringLiteral("Error"), QString("Failed to load FPM map file:\n%1").arg(filePath));
        return;
    }

    m_currentMap = map;

    progress.setValue(55);
    progress.setLabelText(QStringLiteral("Analyzing map memory footprint..."));
    QCoreApplication::processEvents();
    m_cachedMemoryReport = MemoryAnalyzer::analyze(m_currentMap);
    m_memoryReportValid = true;

    progress.setValue(70);
    progress.setLabelText(QStringLiteral("Building visibility zones & portals..."));
    QCoreApplication::processEvents();
    m_canvas->setMap(m_currentMap);

    progress.setValue(85);
    progress.setLabelText(QStringLiteral("Populating entity list..."));
    QCoreApplication::processEvents();
    m_searchDock->setCurrentFloor(m_canvas->currentFloor());
    m_searchDock->setMap(m_currentMap);
    m_inspectorDock->clear();

    if (m_visZoneDock) {
        m_visZoneDock->setMap(m_currentMap);
    }

    if (m_portalLeakDialog && m_portalLeakDialog->isVisible()) {
        m_portalLeakDialog->setMap(m_currentMap);
    }
    if (m_memoryAnalyzerDialog && m_memoryAnalyzerDialog->isVisible()) {
        m_memoryAnalyzerDialog->setMap(m_currentMap);
    }

    progress.setValue(95);
    progress.setLabelText(QStringLiteral("Rendering map canvas..."));
    QCoreApplication::processEvents();
    m_canvas->repaint();

    progress.setValue(100);
    progress.close();

    // Save to Recent Maps list in QSettings
    QSettings settings(QStringLiteral("TGC"), QStringLiteral("FPSCMapViewer"));
    QStringList recent = settings.value(QStringLiteral("recentMaps")).toStringList();
    recent.removeAll(filePath);
    recent.prepend(filePath);
    while (recent.size() > 15) {
        recent.removeLast();
    }
    settings.setValue(QStringLiteral("recentMaps"), recent);
    populateRecentMapsMenu();

    updateWindowTitle();
    updateFloorControls();
    updateStatusBar();
    statusBar()->showMessage(QString("Loaded \"%1\" (%2 entities, %3 segments) in %4 ms")
        .arg(map->mapName)
        .arg(map->placedEntities.size())
        .arg(map->segmentsBank.size())
        .arg(timer.elapsed()), 6000);
}

void MainWindow::onOpenRecentMap(const QString& filePath) {
    if (!filePath.isEmpty() && maybeSave()) {
        loadMapFile(filePath);
    }
}

void MainWindow::onOpenMap() {
    if (!maybeSave()) return;
    QString startDir = AssetManager::instance().engineRoot() + "/Files/mapbank";
    QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Open FPS Creator Map"), startDir, QStringLiteral("FPS Creator Project Map (*.fpm);;All Files (*.*)"));
    if (!file.isEmpty()) {
        loadMapFile(file);
    }
}

void MainWindow::onReloadMap() {
    if (m_currentMap && !m_currentMap->filePath.isEmpty()) {
        if (!maybeSave()) return;
        loadMapFile(m_currentMap->filePath);
    }
}

void MainWindow::onConfigureEnginePath() {
    QString current = AssetManager::instance().engineRoot();
    QString chosen = QFileDialog::getExistingDirectory(this, QStringLiteral("Select FPS Creator Installation Directory"), current);
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
        QMessageBox::information(this, QStringLiteral("No Map Loaded"), QStringLiteral("Please open an FPS Creator map (.FPM) first."));
        return;
    }

    if (!m_memoryAnalyzerDialog) {
        m_memoryAnalyzerDialog = new MemoryAnalyzerDialog(m_currentMap, this);
        m_memoryAnalyzerDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_memoryAnalyzerDialog->show();
    } else {
        m_memoryAnalyzerDialog->setMap(m_currentMap);
        m_memoryAnalyzerDialog->raise();
        m_memoryAnalyzerDialog->activateWindow();
        m_memoryAnalyzerDialog->show();
    }
}

void MainWindow::updateFloorControls() {
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
        QString label = QString("Floor %1 (Y: %2..%3)").arg(l).arg(l * 100).arg((l + 1) * 100);
        if (entCounts[l] > 0 || segCounts[l] > 0) {
            label += QString(" — %1 ents, %2 segs").arg(entCounts[l]).arg(segCounts[l]);
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
    if (!m_currentMap) {
        m_statusMapName->setText(QStringLiteral("No map loaded"));
        m_statusFloor->setText(QStringLiteral("Floor: 0"));
        m_statusMemory->setText(QStringLiteral("RAM: 0 MB"));
        return;
    }

    m_statusMapName->setText(QString("Map: %1 (%2 entities, %3 segments)")
        .arg(m_currentMap->mapName)
        .arg(m_currentMap->placedEntities.size())
        .arg(m_currentMap->segmentsBank.size()));

    m_statusFloor->setText(QString("Floor: %1 / %2 (Height: %3 units)")
        .arg(m_canvas->currentFloor())
        .arg(m_currentMap->header.layerMax)
        .arg(m_canvas->currentFloor() * 100));

    if (m_memoryReportValid) {
        m_statusMemory->setText(QString("Level RAM: %1 MB (%2%)")
            .arg(m_cachedMemoryReport.totalEstimatedRamBytes / (1024.0 * 1024.0), 0, 'f', 1)
            .arg(m_cachedMemoryReport.engineLimitPercent, 0, 'f', 1));
    } else {
        m_statusMemory->setText(QStringLiteral("Level RAM: --"));
    }
}

void MainWindow::onOpenPortalLeakDetector() {
    if (!m_currentMap) {
        QMessageBox::warning(this, "Error", "Please open a map first.");
        return;
    }
    if (!m_portalLeakDialog) {
        m_portalLeakDialog = new PortalLeakDialog(m_currentMap, this);
        m_portalLeakDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_portalLeakDialog, &PortalLeakDialog::cellSelected, m_canvas, &MapCanvas::highlightCell);
        m_portalLeakDialog->show();
    } else {
        m_portalLeakDialog->setMap(m_currentMap);
        m_portalLeakDialog->raise();
        m_portalLeakDialog->activateWindow();
        m_portalLeakDialog->show();
    }

    // Also auto-refresh canvas portals
    PortalLeakAnalyzer analyzer(m_currentMap);
    analyzer.analyze();
    m_canvas->setPortals(analyzer.allPortals(), analyzer.allZones());
}
