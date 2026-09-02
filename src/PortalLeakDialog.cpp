#include "PortalLeakDialog.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>

PortalLeakDialog::PortalLeakDialog(std::shared_ptr<FPSCMap> map, QWidget* parent)
    : QDialog(parent), m_map(map)
{
    setWindowTitle("Portal Leak Detector");
    resize(800, 400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_btnRun = new QPushButton("Run Analysis", this);
    layout->addWidget(m_btnRun);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Severity", "Type", "Location", "Description"});
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    connect(m_btnRun, &QPushButton::clicked, this, &PortalLeakDialog::runAnalysis);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &PortalLeakDialog::onCellDoubleClicked);
}

void PortalLeakDialog::runAnalysis() {
    if (!m_map) {
        QMessageBox::warning(this, "Error", "No map loaded.");
        return;
    }

    m_table->setRowCount(0);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    PortalLeakAnalyzer analyzer(m_map);
    m_currentWarnings = analyzer.analyze();
    
    QApplication::restoreOverrideCursor();

    m_table->setRowCount(m_currentWarnings.size());
    for (size_t i = 0; i < m_currentWarnings.size(); ++i) {
        const auto& w = m_currentWarnings[i];
        
        QTableWidgetItem* sevItem = new QTableWidgetItem(w.severity == PortalLeakWarning::ERROR ? "ERROR" : "WARNING");
        sevItem->setForeground(w.severity == PortalLeakWarning::ERROR ? Qt::red : Qt::yellow);
        
        QTableWidgetItem* typeItem = new QTableWidgetItem(w.type);
        QTableWidgetItem* locItem = new QTableWidgetItem(QString("Layer %1 (%2, %3)").arg(w.layer).arg(w.x).arg(w.y));
        QTableWidgetItem* descItem = new QTableWidgetItem(w.description);

        m_table->setItem(i, 0, sevItem);
        m_table->setItem(i, 1, typeItem);
        m_table->setItem(i, 2, locItem);
        m_table->setItem(i, 3, descItem);
    }
    
    QMessageBox::information(this, "Analysis Complete", QString("Found %1 potential portal leaks.").arg(m_currentWarnings.size()));
}

void PortalLeakDialog::onCellDoubleClicked(int row, int /*column*/) {
    if (row >= 0 && row < (int)m_currentWarnings.size()) {
        const auto& w = m_currentWarnings[row];
        emit cellSelected(w.layer, w.x, w.y);
    }
}
