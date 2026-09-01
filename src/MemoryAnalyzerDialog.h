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

class MemoryAnalyzerDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemoryAnalyzerDialog(std::shared_ptr<FPSCMap> map, QWidget* parent = nullptr);
    ~MemoryAnalyzerDialog() override = default;

private slots:
    void onSearchChanged(const QString& text);
    void onCopyReport();
    void onExportCSV();

private:
    void populateUI();
    QWidget* createCard(const QString& title, const QString& value, const QString& color);

    std::shared_ptr<FPSCMap> m_map;
    MemoryReport m_report;

    QLabel* m_limitLabel = nullptr;
    QProgressBar* m_limitProgress = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QTextEdit* m_tipsEdit = nullptr;

    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
};

#endif // MEMORYANALYZERDIALOG_H
