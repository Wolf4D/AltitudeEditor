#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QScrollArea>
#include <QFrame>
#include <QVBoxLayout>
#include <memory>
#include <vector>
#include "FPSCData.h"
#include "VisZoneManager.h"
#include "PortalLeakAnalyzer.h"

class ZoneVisibilityDialog : public QDialog {
    Q_OBJECT
public:
    explicit ZoneVisibilityDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr, int initialZoneId = 0, QWidget* parent = nullptr);

    void setOriginZone(int zoneId);
    void refreshGraph();

signals:
    void cellSelected(int layer, int x, int y);
    void editSegmentRequested(int layer, int x, int y);
    void tracePathSelected(const std::vector<ZoneConnection>& path);

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public slots:
    void onOriginZoneChanged(int index);
    void onTabBreachesClicked();
    void onTabDoorsClicked();
    void onTabAllClicked();
    void onSearchTextChanged(const QString& text);
    void onZoneSelectionChanged();
    void onZoneTableDoubleClicked(int row, int col);
    void onJumpToCulpritClicked();
    void onEditWallAtCulpritClicked();
    void onFocusOriginZoneClicked();

    QTableWidget* reachableTable() const { return m_tableZones; }
    int currentOriginZoneId() const { return m_currentOriginZoneId; }

private:
    enum class FilterTab {
        BreachesOnly,
        DoorsOnly,
        AllZones
    };

    void setupUi();
    void retranslateUi();
    void populateOriginZones();
    void updateKpiBadges();
    void updateZoneList();
    void updateChainFlow(int targetZoneId);
    int getSelectedTargetZoneId() const;
    const ZoneConnection* findCulpritInChain(const std::vector<ZoneConnection>& chain) const;
    void updateTabButtons();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_visZoneManager;
    PortalLeakAnalyzer m_analyzer;
    QMap<int, ZonePvsInfo> m_pvsGraph;
    int m_currentOriginZoneId = 0;
    FilterTab m_activeTab = FilterTab::BreachesOnly;

    // Header Controls
    QComboBox* m_cmbOriginZone = nullptr;
    QPushButton* m_btnRefresh = nullptr;
    QPushButton* m_btnFocusOrigin = nullptr;

    // KPI Badges
    QFrame* m_badgeStatusFrame = nullptr;
    QLabel* m_lblBadgeStatus = nullptr;
    QFrame* m_badgeTotalFrame = nullptr;
    QLabel* m_lblBadgeTotal = nullptr;
    QFrame* m_badgeDoorsFrame = nullptr;
    QLabel* m_lblBadgeDoors = nullptr;

    // Left Pane Controls
    QPushButton* m_btnTabBreaches = nullptr;
    QPushButton* m_btnTabDoors = nullptr;
    QPushButton* m_btnTabAll = nullptr;
    QLineEdit* m_searchFilter = nullptr;
    QTableWidget* m_tableZones = nullptr;

    // Right Pane Controls
    QLabel* m_lblChainTitle = nullptr;
    QLabel* m_lblChainSubtitle = nullptr;
    QFrame* m_frameDiagnosis = nullptr;
    QLabel* m_lblDiagnosisIcon = nullptr;
    QLabel* m_lblDiagnosisText = nullptr;
    QScrollArea* m_scrollChain = nullptr;
    QWidget* m_chainWidget = nullptr;
    QVBoxLayout* m_chainLayout = nullptr;

    // Bottom Action Bar
    QPushButton* m_btnJumpToCulprit = nullptr;
    QPushButton* m_btnEditCulprit = nullptr;
    QPushButton* m_btnClose = nullptr;

    // Filtered row mapping: row in m_tableZones -> targetZoneId
    std::vector<int> m_visibleTargetZoneIds;
};
