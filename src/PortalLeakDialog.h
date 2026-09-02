#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
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

private:
    std::shared_ptr<FPSCMap> m_map;
    QTableWidget* m_table;
    QPushButton* m_btnRun;
    std::vector<PortalLeakWarning> m_currentWarnings;
};
