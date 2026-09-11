#pragma once

#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QFrame>
#include <QGroupBox>
#include <memory>
#include <vector>
#include "FPSCData.h"
#include "VisZoneManager.h"
#include "PortalLeakAnalyzer.h"

class VisZoneDock : public QDockWidget {
    Q_OBJECT
public:
    explicit VisZoneDock(QWidget* parent = nullptr);

    void setVisZoneManager(std::shared_ptr<VisZoneManager> mgr);
    void setMap(std::shared_ptr<FPSCMap> map);

    int activeZoneId() const { return m_activeZoneId; }
    int currentTab() const { return 0; }
    void setTab(int) {}

public slots:
    void onFloorChanged(int floor);
    void onExternalZoneSelected(int zoneId);
    void resetToNormalView();
    void setColorAllZones(bool enabled);
    bool isColorAllZones() const;
    void refreshGraph();
    void onIsolationOptionChanged();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void zoneSelected(int zoneId);
    void isolationChanged(bool isolate, float dimOpacity);
    void entitySelected(int entityIndex);
    void colorAllZonesToggled(bool enabled);
    void dichotomyDeleteZoneRequested(int zoneId);
    void traceVisibilityRequested(int zoneId);
    void cellSelected(int layer, int x, int y);
    void editSegmentRequested(int layer, int x, int y);
    void tracePathSelected(const std::vector<ZoneConnection>& path);
    void portalHighlighted(const HighlightedPortalInfo& info);
    void portalHighlightCleared();

private slots:
    void onZoneComboChanged(int index);
    void onPrevZone();
    void onNextZone();
    void onFocusZoneClicked();
    void onFloorFilterToggled(bool checked);

    // Portals Slots
    void onPortalClicked(QListWidgetItem* item);
    void onPortalDoubleClicked(QListWidgetItem* item);
    void onVisibleZoneChipClicked(int portalIdx, int vzId);
    void onShowOnMapClicked();
    void onEditWallClicked();

private:
    void setupUi();
    void retranslateUi();
    void populateZoneCombo();
    void updateActiveZoneDetails();
    void updateItemSizeHints();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_mgr;
    PortalLeakAnalyzer m_analyzer;
    QMap<int, ZonePvsInfo> m_pvsGraph;

    int m_currentFloor = 0;
    int m_activeZoneId = -1;
    bool m_updatingCombo = false;
    bool m_refreshingGraph = false;

    std::vector<HighlightedPortalInfo> m_currentPortals;

    // Header Controls
    QPushButton* m_btnPrev = nullptr;
    QComboBox* m_zoneCombo = nullptr;
    QPushButton* m_btnNext = nullptr;
    QPushButton* m_btnFocus = nullptr;
    QPushButton* m_btnResetZone = nullptr;
    QCheckBox* m_chkCurrentFloorOnly = nullptr;
    QPushButton* m_btnRefresh = nullptr;
    QPushButton* m_btnReset = nullptr;

    // Portals & Visibility (Direct layout, no tab widget)
    QLabel* m_lblKpi = nullptr;
    QListWidget* m_listPortals = nullptr;
    QPushButton* m_btnShowOnMap = nullptr;
    QPushButton* m_btnEditWall = nullptr;

    QGroupBox* m_grpIsolation = nullptr;
    QCheckBox* m_chkIsolate = nullptr;
    QLabel* m_lblDim = nullptr;
    QSlider* m_sliderDim = nullptr;
    QCheckBox* m_chkColorAll = nullptr;
    QPushButton* m_btnRecolor = nullptr;
};

