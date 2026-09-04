#include "PortalLeakDialog.h"
#include "Version.h"
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
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Portal & CSG Leak Detector — %2").arg(VersionInfo::shortTitle(), mapName));
    resize(900, 500);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // 1. Group Box with toggles for the two methods
    m_grpMethods = new QGroupBox(tr("Geometry & Leak Detection Methods"), this);
    QVBoxLayout* methodsLayout = new QVBoxLayout(m_grpMethods);
    methodsLayout->setSpacing(6);

    // Method 1: Compiled BSP Universe
    QHBoxLayout* rowBsp = new QHBoxLayout();
    m_chkCompiledBsp = new QCheckBox(tr("1. Physical Compiled BSP Analysis (universe.dbu)"), m_grpMethods);
    m_chkCompiledBsp->setToolTip(tr("Detects physical gaps, unclosed CSG polyhedra, and see-through portals into the void from compiled universe.dbu (after Test Game in FPS Creator)."));
    rowBsp->addWidget(m_chkCompiledBsp);

    PortalLeakAnalyzer initAnalyzer(m_map);
    auto val = initAnalyzer.validateCompiledUniverse();

    m_chkCompiledBsp->setChecked(val.matchesCurrentMap);

    m_lblDbuStatus = new QLabel(m_grpMethods);
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
    m_chkStaticMap = new QCheckBox(tr("2. Static Grid / Topological Analysis (.FPM / .FPS)"), m_grpMethods);
    m_chkStaticMap->setChecked(true);
    m_chkStaticMap->setToolTip(tr("Checks for unclosed ceiling tiles, outer wall breaches in room perimeters, and CSG overlaps before map compilation."));
    methodsLayout->addWidget(m_chkStaticMap);

    layout->addWidget(m_grpMethods);

    // 2. Control bar (Run button + stats label)
    QHBoxLayout* ctrlLayout = new QHBoxLayout();
    m_btnRun = new QPushButton(tr("🔍 Run Analysis"), this);
    m_btnRun->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #253342; color: #4dc4ff; border: 1px solid #36506c; border-radius: 4px; padding: 7px 16px; }"
                                           "QPushButton:hover { background-color: #314357; color: #80d5ff; }"
                                           "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; }"));
    ctrlLayout->addWidget(m_btnRun);

    m_lblStats = new QLabel(tr("Select detection methods and click \"Run Analysis\"."), this);
    m_lblStats->setStyleSheet(QStringLiteral("color: #9ab0c8; font-size: 12px; margin-left: 10px;"));
    ctrlLayout->addWidget(m_lblStats);
    ctrlLayout->addStretch();
    layout->addLayout(ctrlLayout);

    // 3. Results table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        tr("Severity"),
        tr("Leak Type"),
        tr("Coordinates"),
        tr("Issue Description")
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

void PortalLeakDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void PortalLeakDialog::retranslateUi() {
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Portal & CSG Leak Detector — %2").arg(VersionInfo::shortTitle(), mapName));

    if (m_grpMethods) m_grpMethods->setTitle(tr("Geometry & Leak Detection Methods"));
    if (m_chkCompiledBsp) {
        m_chkCompiledBsp->setText(tr("1. Physical Compiled BSP Analysis (universe.dbu)"));
        m_chkCompiledBsp->setToolTip(tr("Detects physical gaps, unclosed CSG polyhedra, and see-through portals into the void from compiled universe.dbu (after Test Game in FPS Creator)."));
    }
    if (m_chkStaticMap) {
        m_chkStaticMap->setText(tr("2. Static Grid / Topological Analysis (.FPM / .FPS)"));
        m_chkStaticMap->setToolTip(tr("Checks for unclosed ceiling tiles, outer wall breaches in room perimeters, and CSG overlaps before map compilation."));
    }
    if (m_btnRun) m_btnRun->setText(tr("🔍 Run Analysis"));

    if (m_table) {
        m_table->setHorizontalHeaderLabels({
            tr("Severity"),
            tr("Leak Type"),
            tr("Coordinates"),
            tr("Issue Description")
        });
    }

    PortalLeakAnalyzer analyzer(m_map);
    auto val = analyzer.validateCompiledUniverse();
    if (m_lblDbuStatus) m_lblDbuStatus->setText(val.message);

    onMethodToggled();
}

void PortalLeakDialog::onMethodToggled() {
    bool anyEnabled = m_chkCompiledBsp->isChecked() || m_chkStaticMap->isChecked();
    m_btnRun->setEnabled(anyEnabled);
    if (anyEnabled) {
        runAnalysis();
    } else {
        m_table->setRowCount(0);
        m_currentWarnings.clear();
        m_lblStats->setText(tr("Enable at least one detection method to run analysis."));
    }
}

void PortalLeakDialog::runAnalysis() {
    if (!m_map) {
        QMessageBox::warning(this, tr("Error"), tr("Map is not loaded."));
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
        
        QTableWidgetItem* sevItem = new QTableWidgetItem(w.severity == PortalLeakWarning::ERROR ? tr("ERROR") : tr("WARNING"));
        sevItem->setForeground(w.severity == PortalLeakWarning::ERROR ? QColor(255, 75, 75) : QColor(255, 200, 50));
        sevItem->setTextAlignment(Qt::AlignCenter);
        
        QTableWidgetItem* typeItem = new QTableWidgetItem(w.type);
        QTableWidgetItem* locItem = new QTableWidgetItem(tr("Layer %1 (%2, %3)").arg(w.layer).arg(w.x).arg(w.y));
        locItem->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem* descItem = new QTableWidgetItem(w.description);

        m_table->setItem(i, 0, sevItem);
        m_table->setItem(i, 1, typeItem);
        m_table->setItem(i, 2, locItem);
        m_table->setItem(i, 3, descItem);
    }
    
    m_lblStats->setText(tr("Found issues: <b>%1</b> (Physical BSP: %2, Static Grid: %3). Double-click jumps camera to tile.")
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
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Portal & CSG Leak Detector — %2").arg(VersionInfo::shortTitle(), mapName));

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

