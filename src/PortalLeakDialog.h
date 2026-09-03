#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <memory>
#include "FPSCData.h"
#include "PortalLeakAnalyzer.h"

class PortalLeakDialog : public QDialog {
    Q_OBJECT
public:
    explicit PortalLeakDialog(std::shared_ptr<FPSCMap> map, QWidget* parent = nullptr);

signals:
    void cellSelected(int layer, int x, int y);

private slots:
    void runAnalysis();
    void onCellDoubleClicked(int row, int column);
    void onMethodToggled();

private:
    std::shared_ptr<FPSCMap> m_map;
    QCheckBox* m_chkCompiledBsp;
    QCheckBox* m_chkStaticMap;
    QLabel* m_lblDbuStatus;
    QLabel* m_lblStats;
    QPushButton* m_btnRun;
    QTableWidget* m_table;
    std::vector<PortalLeakWarning> m_currentWarnings;
};
