#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <memory>
#include "FPSCData.h"
#include "PortalLeakAnalyzer.h"
#include <QComboBox>
#include <QMenu>

class VisZoneManager;

class PortalLeakDialog : public QDialog {
    Q_OBJECT
public:
    explicit PortalLeakDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr = nullptr, QWidget* parent = nullptr);

    void setMap(std::shared_ptr<FPSCMap> map);
    void setVisZoneManager(std::shared_ptr<VisZoneManager> mgr);

protected:
    void changeEvent(QEvent* event) override;

signals:
    void cellSelected(int layer, int x, int y);
    void resolveConflictRequested(int layer, int x1, int y1, int x2, int y2);
    void editSegmentRequested(int layer, int x, int y);

public slots:
    void runAnalysis();

private slots:
    void onCellDoubleClicked(int row, int column);
    void onMethodToggled();
    void onZoneFilterChanged(int index);
    void onTableContextMenu(const QPoint& pos);
    void onResolveClicked();
    void onTableSelectionChanged();

private:
    void retranslateUi();
    void updateTableRows();
    void populateZoneFilter();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_visZoneManager;
    class QGroupBox* m_grpMethods = nullptr;
    QCheckBox* m_chkCompiledBsp = nullptr;
    QCheckBox* m_chkStaticMap = nullptr;
    QLabel* m_lblDbuStatus = nullptr;
    QLabel* m_lblStats = nullptr;
    QPushButton* m_btnRun = nullptr;
    QLabel* m_lblZoneFilter = nullptr;
    QComboBox* m_cmbZoneFilter = nullptr;
    QPushButton* m_btnResolve = nullptr;
    QTableWidget* m_table = nullptr;
    std::vector<PortalLeakWarning> m_currentWarnings;
    std::vector<int> m_visibleWarningIndices;
    int m_selectedZoneFilterId = -1; // -1 = All, -2 = Unzoned / Void, >= 0 = zoneId
};
