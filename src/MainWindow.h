#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FPSCData.h"
#include "MapCanvas.h"
#include "EntitySearchDock.h"
#include "EntityInspector.h"
#include <QMainWindow>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QAction>
#include <memory>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void loadMapFile(const QString& filePath);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onOpenMap();
    void onSaveMap();
    void onSaveMapAs();
    void onOpenRecentMap(const QString& filePath);
    void onReloadMap();
    void onConfigureEnginePath();
    void onOpenMemoryAnalyzer();

    void onFloorComboChanged(int index);
    void onFloorSpinChanged(int value);
    void onFloorSliderChanged(int value);
    void onCanvasFloorChanged(int floor);

    void onEntitySelected(int index);
    void onEntityModified(int index);
    void onFocusEntityRequested(int index);
    void deleteEntity(int index);
    void onHoverInfoChanged(const QString& info);
    void onZoomChanged(float zoom);

private:
    bool maybeSave();
    void updateWindowTitle();
    void createMenusAndToolbars();
    void populateRecentMapsMenu();
    void updateFloorControls();
    void updateStatusBar();

    std::shared_ptr<FPSCMap> m_currentMap;

    MapCanvas* m_canvas = nullptr;
    EntitySearchDock* m_searchDock = nullptr;
    EntityInspector* m_inspectorDock = nullptr;

    // Floor UI Controls
    QComboBox* m_floorCombo = nullptr;
    QSpinBox* m_floorSpin = nullptr;
    QSlider* m_floorSlider = nullptr;
    QLabel* m_floorLabel = nullptr;
    QAction* m_actSave = nullptr;
    QAction* m_actSaveAs = nullptr;
    QAction* m_actFloorUp = nullptr;
    QAction* m_actFloorDown = nullptr;
    QAction* m_actGhostLayer = nullptr;

    // View Action Toggles
    QAction* m_actWallTex = nullptr;
    QAction* m_actFloorTex = nullptr;
    QAction* m_actGrid = nullptr;
    QAction* m_actEntities = nullptr;
    QAction* m_actLights = nullptr;
    QAction* m_actZones = nullptr;
    QAction* m_actWaypoints = nullptr;

    // Status Bar Labels
    QLabel* m_statusMapName = nullptr;
    QLabel* m_statusFloor = nullptr;
    QLabel* m_statusCoords = nullptr;
    QLabel* m_statusMemory = nullptr;

    QMenu* m_recentMapsMenu = nullptr;
    bool m_isUpdatingFloorUI = false;
};

#endif // MAINWINDOW_H
