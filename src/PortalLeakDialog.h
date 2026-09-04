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

    void setMap(std::shared_ptr<FPSCMap> map);

protected:
    void changeEvent(QEvent* event) override;

signals:
    void cellSelected(int layer, int x, int y);

private slots:
    void runAnalysis();
    void onCellDoubleClicked(int row, int column);
    void onMethodToggled();

private:
    void retranslateUi();

    std::shared_ptr<FPSCMap> m_map;
    class QGroupBox* m_grpMethods = nullptr;
    QCheckBox* m_chkCompiledBsp = nullptr;
    QCheckBox* m_chkStaticMap = nullptr;
    QLabel* m_lblDbuStatus = nullptr;
    QLabel* m_lblStats = nullptr;
    QPushButton* m_btnRun = nullptr;
    QTableWidget* m_table = nullptr;
    std::vector<PortalLeakWarning> m_currentWarnings;
};
