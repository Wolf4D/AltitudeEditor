#include "PortalLeakDialog.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QFile>

PortalLeakDialog::PortalLeakDialog(std::shared_ptr<FPSCMap> map, QWidget* parent)
    : QDialog(parent), m_map(map)
{
    QString mapName = m_map ? m_map->mapName : QStringLiteral("No Map");
    setWindowTitle(QString("Детектор утечек порталов и разрывов CSG — %1").arg(mapName));
    resize(900, 500);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // 1. Group Box with toggles for the two methods
    QGroupBox* grpMethods = new QGroupBox(QStringLiteral("Методы проверки геометрии и утечек (Detection Methods)"), this);
    QVBoxLayout* methodsLayout = new QVBoxLayout(grpMethods);
    methodsLayout->setSpacing(6);

    // Method 1: Compiled BSP Universe
    QHBoxLayout* rowBsp = new QHBoxLayout();
    m_chkCompiledBsp = new QCheckBox(QStringLiteral("1. Физический анализ скомпилированного BSP (universe.dbu)"), grpMethods);
    m_chkCompiledBsp->setToolTip(QStringLiteral("Проверяет реальные физические щели, несомкнутые многогранники CSG и сквозные порталы в пустоту из скомпилированного universe.dbu (после запуска Test Game в FPS Creator)."));
    rowBsp->addWidget(m_chkCompiledBsp);

    PortalLeakAnalyzer initAnalyzer(m_map);
    auto val = initAnalyzer.validateCompiledUniverse();

    m_chkCompiledBsp->setChecked(val.matchesCurrentMap);

    m_lblDbuStatus = new QLabel(grpMethods);
    m_lblDbuStatus->setText(val.message);
    if (!val.fileExists) {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #ff9100; font-size: 11px;"));
    } else if (!val.matchesCurrentMap) {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #ff5252; font-weight: bold; font-size: 11px;"));
    } else if (val.isOutdated) {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #ffb300; font-size: 11px;"));
    } else {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #00e676; font-weight: bold; font-size: 11px;"));
    }
    rowBsp->addStretch();
    rowBsp->addWidget(m_lblDbuStatus);
    methodsLayout->addLayout(rowBsp);

    // Method 2: Static Map Analysis
    m_chkStaticMap = new QCheckBox(QStringLiteral("2. Статический сеточный / топологический анализ (.FPM / .FPS)"), grpMethods);
    m_chkStaticMap->setChecked(true);
    m_chkStaticMap->setToolTip(QStringLiteral("Проверяет незакрытые потолочные плиты, внешние пробоины в стенах периметра комнат и наложения CSG до компиляции карты."));
    methodsLayout->addWidget(m_chkStaticMap);

    layout->addWidget(grpMethods);

    // 2. Control bar (Run button + stats label)
    QHBoxLayout* ctrlLayout = new QHBoxLayout();
    m_btnRun = new QPushButton(QStringLiteral("🔍 Запустить анализ (Run Analysis)"), this);
    m_btnRun->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #253342; color: #4dc4ff; border: 1px solid #36506c; border-radius: 4px; padding: 7px 16px; }"
                                           "QPushButton:hover { background-color: #314357; color: #80d5ff; }"
                                           "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; }"));
    ctrlLayout->addWidget(m_btnRun);

    m_lblStats = new QLabel(QStringLiteral("Выберите методы и нажмите «Запустить анализ»."), this);
    m_lblStats->setStyleSheet(QStringLiteral("color: #9ab0c8; font-size: 12px; margin-left: 10px;"));
    ctrlLayout->addWidget(m_lblStats);
    ctrlLayout->addStretch();
    layout->addLayout(ctrlLayout);

    // 3. Results table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Серьёзность"),
        QStringLiteral("Тип утечки"),
        QStringLiteral("Координаты"),
        QStringLiteral("Описание проблемы")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);

    // Connections
    connect(m_chkCompiledBsp, &QCheckBox::toggled, this, &PortalLeakDialog::onMethodToggled);
    connect(m_chkStaticMap, &QCheckBox::toggled, this, &PortalLeakDialog::onMethodToggled);
    connect(m_btnRun, &QPushButton::clicked, this, &PortalLeakDialog::runAnalysis);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &PortalLeakDialog::onCellDoubleClicked);

    // Auto-run initial analysis
    runAnalysis();
}

void PortalLeakDialog::onMethodToggled() {
    bool anyEnabled = m_chkCompiledBsp->isChecked() || m_chkStaticMap->isChecked();
    m_btnRun->setEnabled(anyEnabled);
    if (anyEnabled) {
        runAnalysis();
    } else {
        m_table->setRowCount(0);
        m_currentWarnings.clear();
        m_lblStats->setText(QStringLiteral("Включите хотя бы один метод проверки для анализа."));
    }
}

void PortalLeakDialog::runAnalysis() {
    if (!m_map) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Карта не загружена."));
        return;
    }

    m_table->setRowCount(0);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    PortalLeakAnalyzer analyzer(m_map);
    analyzer.setCheckCompiledUniverse(m_chkCompiledBsp->isChecked());
    analyzer.setCheckStaticMap(m_chkStaticMap->isChecked());
    m_currentWarnings = analyzer.analyze();
    
    QApplication::restoreOverrideCursor();

    int bspCount = 0;
    int staticCount = 0;

    m_table->setRowCount(m_currentWarnings.size());
    for (size_t i = 0; i < m_currentWarnings.size(); ++i) {
        const auto& w = m_currentWarnings[i];
        if (w.type.contains(QStringLiteral("BSP")) || w.type.contains(QStringLiteral("Universe"))) {
            bspCount++;
        } else {
            staticCount++;
        }
        
        QTableWidgetItem* sevItem = new QTableWidgetItem(w.severity == PortalLeakWarning::ERROR ? QStringLiteral("ERROR") : QStringLiteral("WARNING"));
        sevItem->setForeground(w.severity == PortalLeakWarning::ERROR ? QColor(255, 75, 75) : QColor(255, 200, 50));
        sevItem->setTextAlignment(Qt::AlignCenter);
        
        QTableWidgetItem* typeItem = new QTableWidgetItem(w.type);
        QTableWidgetItem* locItem = new QTableWidgetItem(QString("Layer %1 (%2, %3)").arg(w.layer).arg(w.x).arg(w.y));
        locItem->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem* descItem = new QTableWidgetItem(w.description);

        m_table->setItem(i, 0, sevItem);
        m_table->setItem(i, 1, typeItem);
        m_table->setItem(i, 2, locItem);
        m_table->setItem(i, 3, descItem);
    }
    
    m_lblStats->setText(QString("Найдено проблем: <b>%1</b> (Физических BSP: %2, Статических сеточных: %3). Двойной клик переносит камеру к ячейке.")
                        .arg(m_currentWarnings.size()).arg(bspCount).arg(staticCount));
}

void PortalLeakDialog::onCellDoubleClicked(int row, int /*column*/) {
    if (row >= 0 && row < (int)m_currentWarnings.size()) {
        const auto& w = m_currentWarnings[row];
        emit cellSelected(w.layer, w.x, w.y);
    }
}

void PortalLeakDialog::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    QString mapName = m_map ? m_map->mapName : QStringLiteral("No Map");
    setWindowTitle(QString("Детектор утечек порталов и разрывов CSG — %1").arg(mapName));

    PortalLeakAnalyzer analyzer(m_map);
    auto val = analyzer.validateCompiledUniverse();
    m_chkCompiledBsp->setChecked(val.matchesCurrentMap);
    m_lblDbuStatus->setText(val.message);
    if (!val.fileExists) {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #ff9100; font-size: 11px;"));
    } else if (!val.matchesCurrentMap) {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #ff5252; font-weight: bold; font-size: 11px;"));
    } else if (val.isOutdated) {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #ffb300; font-size: 11px;"));
    } else {
        m_lblDbuStatus->setStyleSheet(QStringLiteral("color: #00e676; font-weight: bold; font-size: 11px;"));
    }

    runAnalysis();
}

