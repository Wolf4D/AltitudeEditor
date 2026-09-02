#include "MainWindow.h"
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
#include <QApplication>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    updateWindowTitle();
    resize(1360, 860);

    m_canvas = new MapCanvas(this);
    setCentralWidget(m_canvas);

    // Left Dock: Entity Search
    m_searchDock = new EntitySearchDock(this);
    m_searchDock->setMinimumWidth(260);
    addDockWidget(Qt::LeftDockWidgetArea, m_searchDock);

    // Right Dock: Entity Inspector
    m_inspectorDock = new EntityInspector(this);
    m_inspectorDock->setMinimumWidth(330);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);

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

    m_recentMapsMenu = fileMenu->addMenu(QStringLiteral("&Stock / Recent Maps"));
    populateRecentMapsMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("&Configure FPS Creator Path..."), this, &MainWindow::onConfigureEnginePath);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &MainWindow::close, QKeySequence::Quit);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    QAction* actZoomIn = viewMenu->addAction(QStringLiteral("Zoom &In"), m_canvas, &MapCanvas::zoomIn, QKeySequence::ZoomIn);
    QAction* actZoomOut = viewMenu->addAction(QStringLiteral("Zoom &Out"), m_canvas, &MapCanvas::zoomOut, QKeySequence::ZoomOut);
    QAction* actZoomReset = viewMenu->addAction(QStringLiteral("Reset Zoom (100%)"), m_canvas, &MapCanvas::zoomReset, Qt::Key_0);
    QAction* actZoomFit = viewMenu->addAction(QStringLiteral("Fit Level in View"), m_canvas, &MapCanvas::zoomFit, Qt::Key_Home);

    viewMenu->addSeparator();
    m_actWallTex = viewMenu->addAction(QStringLiteral("Show &Wall Textures (Doom Style)"));
    m_actWallTex->setCheckable(true);
    m_actWallTex->setChecked(true);
    connect(m_actWallTex, &QAction::toggled, m_canvas, &MapCanvas::setShowWallTextures);

    m_actFloorTex = viewMenu->addAction(QStringLiteral("Show &Floor Textures"));
    m_actFloorTex->setCheckable(true);
    m_actFloorTex->setChecked(true);
    connect(m_actFloorTex, &QAction::toggled, m_canvas, &MapCanvas::setShowFloorTextures);

    m_actGrid = viewMenu->addAction(QStringLiteral("Show &Grid Lines"));
    m_actGrid->setCheckable(true);
    m_actGrid->setChecked(true);
    connect(m_actGrid, &QAction::toggled, m_canvas, &MapCanvas::setShowGrid);

    m_actEntities = viewMenu->addAction(QStringLiteral("Show &Entities (.BMP Icons)"));
    m_actEntities->setCheckable(true);
    m_actEntities->setChecked(true);
    connect(m_actEntities, &QAction::toggled, m_canvas, &MapCanvas::setShowEntities);

    m_actLights = viewMenu->addAction(QStringLiteral("Show &Light Halos"));
    m_actLights->setCheckable(true);
    m_actLights->setChecked(true);
    connect(m_actLights, &QAction::toggled, m_canvas, &MapCanvas::setShowLights);

    m_actZones = viewMenu->addAction(QStringLiteral("Show &Trigger Zones"));
    m_actZones->setCheckable(true);
    m_actZones->setChecked(true);
    connect(m_actZones, &QAction::toggled, m_canvas, &MapCanvas::setShowZones);

    m_actWaypoints = viewMenu->addAction(QStringLiteral("Show &Waypoints / AI Paths"));
    m_actWaypoints->setCheckable(true);
    m_actWaypoints->setChecked(true);
    connect(m_actWaypoints, &QAction::toggled, m_canvas, &MapCanvas::setShowWaypoints);

    m_actGhostLayer = viewMenu->addAction(QStringLiteral("Show &Ghost Lower Floor"));
    m_actGhostLayer->setCheckable(true);
    m_actGhostLayer->setChecked(true);
    connect(m_actGhostLayer, &QAction::toggled, m_canvas, &MapCanvas::setShowGhostLayer);

    m_actShowPortals = viewMenu->addAction(QStringLiteral("Show &Portals / VisZones (DBU)"));
    m_actShowPortals->setCheckable(true);
    m_actShowPortals->setChecked(false);
    m_actShowPortals->setToolTip(QStringLiteral("Render BSP Portals and VisZone bounding boxes from compiled universe.dbu"));
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

    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));
    QAction* actMem = toolsMenu->addAction(QStringLiteral("&Entity Memory Analyzer (MB)..."), this, &MainWindow::onOpenMemoryAnalyzer, QKeySequence(Qt::CTRL + Qt::Key_M));
    toolsMenu->addAction(QStringLiteral("&Portal Leak Detector..."), this, &MainWindow::onOpenPortalLeakDetector);

    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("&About FPS Creator Map Viewer..."), this, [this]() {
        QMessageBox::about(this, QStringLiteral("About FPS Creator 2D Map Viewer"),
            QStringLiteral("<h3>FPS Creator 2D Map Viewer</h3>"
                           "<p>A high-fidelity 2D map viewer for <b>FPS Creator</b> maps (.FPM).</p>"
                           "<ul>"
                           "<li><b>Doom-Style Segment Wall Texturing:</b> Displays textured wall strips and floor tiles with cell rotations.</li>"
                           "<li><b>Floor-by-Floor Navigation:</b> Full layer switching (0..20) via toolbar, shortcuts (PageUp/PageDown), and mouse wheel.</li>"
                           "<li><b>Entity Icons & Search:</b> Renders .BMP icons and provides instant entity filtering by name and category.</li>"
                           "<li><b>Memory Footprint Analyzer:</b> Measures memory weight in MB for 3D meshes, textures, and audio buffers with 32-bit limit warnings.</li>"
                           "</ul>"
                           "<p>Built with <b>Qt 5.15.2 (MinGW 32-bit)</b>.</p>"));
    });

    // -------------------------------------------------------------
    // Main Toolbar
    // -------------------------------------------------------------
    QToolBar* mainBar = addToolBar(QStringLiteral("Main Controls"));
    mainBar->setObjectName("MainToolBar");
    mainBar->setMovable(false);

    mainBar->addAction(actOpen);
    mainBar->addAction(m_actSave);
    mainBar->addAction(actReload);
    mainBar->addSeparator();

    // Floor Navigation Controls
    m_actFloorDown = mainBar->addAction(QStringLiteral("▼ Lower Floor"), m_canvas, &MapCanvas::floorDown);
    m_actFloorDown->setShortcuts({QKeySequence(Qt::Key_PageDown), QKeySequence(Qt::Key_Minus), QKeySequence(Qt::Key_Underscore)});
    m_actFloorDown->setToolTip(QStringLiteral("Go one floor down (PageDown / -)"));

    m_floorCombo = new QComboBox(this);
    m_floorCombo->setMinimumWidth(170);
    mainBar->addWidget(m_floorCombo);
    connect(m_floorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFloorComboChanged);

    m_floorSpin = new QSpinBox(this);
    m_floorSpin->setRange(0, 20);
    m_floorSpin->setPrefix("Floor: ");
    mainBar->addWidget(m_floorSpin);
    connect(m_floorSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onFloorSpinChanged);

    m_actFloorUp = mainBar->addAction(QStringLiteral("▲ Upper Floor"), m_canvas, &MapCanvas::floorUp);
    m_actFloorUp->setShortcuts({QKeySequence(Qt::Key_PageUp), QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_Equal)});
    m_actFloorUp->setToolTip(QStringLiteral("Go one floor up (PageUp / +)"));

    mainBar->addAction(m_actGhostLayer);
    mainBar->addSeparator();

    // Zoom Controls
    mainBar->addAction(actZoomIn);
    mainBar->addAction(actZoomOut);
    mainBar->addAction(actZoomFit);
    mainBar->addSeparator();

    // View Toggles
    mainBar->addAction(m_actWallTex);
    mainBar->addAction(m_actFloorTex);
    mainBar->addAction(m_actEntities);
    mainBar->addAction(m_actShowPortals);
    mainBar->addSeparator();

    // Memory Analyzer Launch Button
    QAction* actLaunchMem = mainBar->addAction(QStringLiteral("💾 Entity Memory Analyzer (MB)"), this, &MainWindow::onOpenMemoryAnalyzer);
    actLaunchMem->setToolTip(QStringLiteral("Measure entity RAM weight in Megabytes and inspect memory budget"));

    // Portal Leak Detector Button
    QAction* actLaunchPortals = mainBar->addAction(QStringLiteral("🔍 Portal Leak Detector"), this, &MainWindow::onOpenPortalLeakDetector);
    actLaunchPortals->setToolTip(QStringLiteral("Scan compiled universe.dbu and map geometry for portal occlusion leaks"));
}

void MainWindow::updateWindowTitle() {
    QString title = QStringLiteral("FPS Creator 2D Map Viewer (Doom-Style Segment Textures & Memory Analyzer)");
    if (m_currentMap) {
        QString fName = QFileInfo(m_currentMap->filePath).fileName();
        if (fName.isEmpty()) fName = m_currentMap->mapName;
        title = QString("FPS Creator 2D Map Viewer - [%1%2]").arg(fName).arg(m_currentMap->isModified ? "*" : "");
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
    QString mapBankDir = AssetManager::instance().engineRoot() + "/Files/mapbank";
    if (!QDir(mapBankDir).exists()) return;

    QDirIterator it(mapBankDir, {"*.fpm"}, QDir::Files, QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext() && count < 25) {
        QString fPath = it.next();
        QString name = QFileInfo(fPath).fileName();
        QAction* act = m_recentMapsMenu->addAction(name, this, [this, fPath]() {
            if (maybeSave()) {
                loadMapFile(fPath);
            }
        });
        act->setToolTip(fPath);
        count++;
    }
}

void MainWindow::loadMapFile(const QString& filePath) {
    auto map = FPMReader::loadMap(filePath, "mypassword");
    if (!map) {
        QMessageBox::critical(this, QStringLiteral("Error"), QString("Failed to load FPM map file:\n%1").arg(filePath));
        return;
    }

    m_currentMap = map;
    m_canvas->setMap(m_currentMap);
    m_searchDock->setCurrentFloor(m_canvas->currentFloor());
    m_searchDock->setMap(m_currentMap);
    m_inspectorDock->clear();

    updateWindowTitle();
    updateFloorControls();
    updateStatusBar();
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

    MemoryAnalyzerDialog dlg(m_currentMap, this);
    dlg.exec();
}

void MainWindow::updateFloorControls() {
    if (!m_currentMap) return;

    m_isUpdatingFloorUI = true;
    m_floorCombo->clear();

    int layerMax = m_currentMap->header.layerMax;
    m_floorSpin->setRange(0, layerMax);

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
    m_floorSpin->setValue(active);
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
    m_floorSpin->setValue(floor);
    m_isUpdatingFloorUI = false;

    m_searchDock->setCurrentFloor(floor);
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

    // Estimate quick total memory
    qint64 totalBytes = 0;
    for (const auto& prof : m_currentMap->entityProfiles) {
        if (prof) totalBytes += prof->estimatedRAMBytes;
    }
    m_statusMemory->setText(QString("Entity RAM: %1 MB").arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 1));
}

void MainWindow::onOpenPortalLeakDetector() {
    if (!m_currentMap) {
        QMessageBox::warning(this, "Error", "Please open a map first.");
        return;
    }
    PortalLeakDialog* dlg = new PortalLeakDialog(m_currentMap, this);
    connect(dlg, &PortalLeakDialog::cellSelected, m_canvas, &MapCanvas::highlightCell);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();

    // Also auto-refresh canvas portals
    PortalLeakAnalyzer analyzer(m_currentMap);
    analyzer.analyze();
    m_canvas->setPortals(analyzer.allPortals(), analyzer.allZones());
}
