#ifndef MEMORYANALYZERDIALOG_H
#define MEMORYANALYZERDIALOG_H

#include "MemoryAnalyzer.h"
#include <QDialog>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QTabWidget>

class MemoryAnalyzerDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemoryAnalyzerDialog(std::shared_ptr<FPSCMap> map, QWidget* parent = nullptr);
    ~MemoryAnalyzerDialog() override = default;

    void setMap(std::shared_ptr<FPSCMap> map);

private slots:
    void onSearchChanged(const QString& text);
    void onCopyReport();
    void onExportCSV();

private:
    void populateUI();
    QWidget* createCard(const QString& title, QLabel*& outValueLabel, const QString& color);

    std::shared_ptr<FPSCMap> m_map;
    MemoryReport m_report;

    QLabel* m_cardTotalVal = nullptr;
    QLabel* m_cardSegVal = nullptr;
    QLabel* m_cardEntVal = nullptr;
    QLabel* m_cardUniVal = nullptr;
    QLabel* m_cardEngineVal = nullptr;

    QLabel* m_limitLabel = nullptr;
    QProgressBar* m_limitProgress = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QTableWidget* m_entityTable = nullptr;
    QTableWidget* m_segmentTable = nullptr;
    QTableWidget* m_engineTable = nullptr;
    QTextEdit* m_tipsEdit = nullptr;

    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
};

#endif // MEMORYANALYZERDIALOG_H
