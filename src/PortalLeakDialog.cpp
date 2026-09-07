#include "PortalLeakDialog.h"
#include "Version.h"
#include "AssetManager.h"
#include "VisZoneManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QFile>

PortalLeakDialog::PortalLeakDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr, QWidget* parent)
    : QDialog(parent), m_map(map), m_visZoneManager(visZoneMgr)
{
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Portal & CSG Leak Detector — %2").arg(VersionInfo::shortTitle(), mapName));
    resize(980, 560);

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

    PortalLeakAnalyzer initAnalyzer(m_map, m_visZoneManager);
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

    // 2. Control bar (Run button + VisZone filter + Resolve button + stats label)
    QHBoxLayout* ctrlLayout = new QHBoxLayout();
    m_btnRun = new QPushButton(tr("🔍 Run Analysis"), this);
    m_btnRun->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #253342; color: #4dc4ff; border: 1px solid #36506c; border-radius: 4px; padding: 7px 16px; }"
                                           "QPushButton:hover { background-color: #314357; color: #80d5ff; }"
                                           "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; }"));
    ctrlLayout->addWidget(m_btnRun);

    ctrlLayout->addSpacing(10);
    m_lblZoneFilter = new QLabel(tr("Filter by Vis Zone:"), this);
    m_lblZoneFilter->setStyleSheet(QStringLiteral("color: #cad8e6; font-weight: bold; font-size: 12px;"));
    ctrlLayout->addWidget(m_lblZoneFilter);

    m_cmbZoneFilter = new QComboBox(this);
    m_cmbZoneFilter->setMinimumWidth(240);
    m_cmbZoneFilter->setStyleSheet(QStringLiteral("QComboBox { background-color: #1e2630; color: #cad8e6; border: 1px solid #3d4f61; border-radius: 4px; padding: 5px 8px; }"
                                                 "QComboBox QAbstractItemView { background-color: #1e2630; color: #cad8e6; selection-background-color: #2e537a; }"));
    ctrlLayout->addWidget(m_cmbZoneFilter);

    ctrlLayout->addSpacing(10);
    m_btnResolve = new QPushButton(tr("⚡ Resolve Clash / Edit..."), this);
    m_btnResolve->setEnabled(false);
    m_btnResolve->setToolTip(tr("Open the Segment Editor & Conflict Resolver for the selected row"));
    m_btnResolve->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #243d26; color: #72f07b; border: 1px solid #3b6b3e; border-radius: 4px; padding: 7px 14px; }"
                                               "QPushButton:hover { background-color: #2f5432; color: #94ff9c; }"
                                               "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; border-color: #2b3846; }"));
    ctrlLayout->addWidget(m_btnResolve);

    ctrlLayout->addStretch();
    layout->addLayout(ctrlLayout);

    // Sub-bar for stats
    QHBoxLayout* statsLayout = new QHBoxLayout();
    m_lblStats = new QLabel(tr("Select detection methods and click \"Run Analysis\"."), this);
    m_lblStats->setStyleSheet(QStringLiteral("color: #9ab0c8; font-size: 12px;"));
    statsLayout->addWidget(m_lblStats);
    statsLayout->addStretch();
    layout->addLayout(statsLayout);

    // 3. Results table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        tr("Severity"),
        tr("Leak Type"),
        tr("Vis Zone"),
        tr("Coordinates"),
        tr("Issue Description")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_table);

    // Connections
    connect(m_chkCompiledBsp, &QCheckBox::toggled, this, &PortalLeakDialog::onMethodToggled);
    connect(m_chkStaticMap, &QCheckBox::toggled, this, &PortalLeakDialog::onMethodToggled);
    connect(m_btnRun, &QPushButton::clicked, this, &PortalLeakDialog::runAnalysis);
    connect(m_cmbZoneFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PortalLeakDialog::onZoneFilterChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &PortalLeakDialog::onCellDoubleClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &PortalLeakDialog::onTableSelectionChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &PortalLeakDialog::onTableContextMenu);
    connect(m_btnResolve, &QPushButton::clicked, this, &PortalLeakDialog::onResolveClicked);

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
    if (m_lblZoneFilter) m_lblZoneFilter->setText(tr("Filter by Vis Zone:"));
    if (m_btnResolve) m_btnResolve->setText(tr("⚡ Resolve Clash / Edit..."));

    if (m_table) {
        m_table->setHorizontalHeaderLabels({
            tr("Severity"),
            tr("Leak Type"),
            tr("Vis Zone"),
            tr("Coordinates"),
            tr("Issue Description")
        });
    }

    PortalLeakAnalyzer analyzer(m_map, m_visZoneManager);
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
        m_visibleWarningIndices.clear();
        m_cmbZoneFilter->clear();
        m_lblStats->setText(tr("Enable at least one detection method to run analysis."));
        m_btnResolve->setEnabled(false);
    }
}

void PortalLeakDialog::runAnalysis() {
    if (!m_map) {
        QMessageBox::warning(this, tr("Error"), tr("Map is not loaded."));
        return;
    }

    m_table->setRowCount(0);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    PortalLeakAnalyzer analyzer(m_map, m_visZoneManager);
    analyzer.setCheckCompiledUniverse(m_chkCompiledBsp->isChecked());
    analyzer.setCheckStaticMap(m_chkStaticMap->isChecked());
    m_currentWarnings = analyzer.analyze();
    m_visZoneManager = analyzer.visZoneManager();
    
    QApplication::restoreOverrideCursor();

    populateZoneFilter();
    updateTableRows();
}

void PortalLeakDialog::populateZoneFilter() {
    m_cmbZoneFilter->blockSignals(true);
    m_cmbZoneFilter->clear();

    QMap<int, int> zoneCounts;
    int unassignedCount = 0;

    for (const auto& w : m_currentWarnings) {
        if (w.zoneId >= 0) {
            zoneCounts[w.zoneId]++;
        }
        if (w.zoneId2 >= 0 && w.zoneId2 != w.zoneId) {
            zoneCounts[w.zoneId2]++;
        }
        if (w.zoneId < 0 && w.zoneId2 < 0) {
            unassignedCount++;
        }
    }

    m_cmbZoneFilter->addItem(tr("All Vis Zones (%1 issues)").arg(m_currentWarnings.size()), -1);

    if (unassignedCount > 0) {
        m_cmbZoneFilter->addItem(tr("Outside / Void / Unzoned (%1)").arg(unassignedCount), -2);
    }

    QList<int> sortedZones = zoneCounts.keys();
    std::sort(sortedZones.begin(), sortedZones.end());

    for (int zid : sortedZones) {
        int count = zoneCounts[zid];
        QString zName = QString("Zone %1").arg(zid + 1);
        if (m_visZoneManager) {
            const VisZone* vz = m_visZoneManager->getZone(zid);
            if (vz) {
                zName = vz->name;
            }
        }
        m_cmbZoneFilter->addItem(tr("%1 — %2 issues").arg(zName).arg(count), zid);
    }

    // Restore selected filter if still present
    int selectIdx = 0;
    for (int i = 0; i < m_cmbZoneFilter->count(); ++i) {
        if (m_cmbZoneFilter->itemData(i).toInt() == m_selectedZoneFilterId) {
            selectIdx = i;
            break;
        }
    }
    m_cmbZoneFilter->setCurrentIndex(selectIdx);
    m_selectedZoneFilterId = m_cmbZoneFilter->itemData(selectIdx).toInt();

    m_cmbZoneFilter->blockSignals(false);
}

void PortalLeakDialog::onZoneFilterChanged(int index) {
    if (index >= 0) {
        m_selectedZoneFilterId = m_cmbZoneFilter->itemData(index).toInt();
        updateTableRows();
    }
}

void PortalLeakDialog::updateTableRows() {
    m_visibleWarningIndices.clear();

    int bspCount = 0;
    int staticCount = 0;

    for (int i = 0; i < static_cast<int>(m_currentWarnings.size()); ++i) {
        const auto& w = m_currentWarnings[i];

        bool match = false;
        if (m_selectedZoneFilterId == -1) {
            match = true;
        } else if (m_selectedZoneFilterId == -2) {
            match = (w.zoneId < 0 && w.zoneId2 < 0);
        } else if (m_selectedZoneFilterId >= 0) {
            match = (w.zoneId == m_selectedZoneFilterId || w.zoneId2 == m_selectedZoneFilterId);
        }

        if (match) {
            m_visibleWarningIndices.push_back(i);
            if (w.type.contains(QStringLiteral("BSP")) || w.type.contains(QStringLiteral("Universe"))) {
                bspCount++;
            } else {
                staticCount++;
            }
        }
    }

    m_table->setRowCount(m_visibleWarningIndices.size());
    for (int row = 0; row < static_cast<int>(m_visibleWarningIndices.size()); ++row) {
        int origIdx = m_visibleWarningIndices[row];
        const auto& w = m_currentWarnings[origIdx];

        QTableWidgetItem* sevItem = new QTableWidgetItem(w.severity == PortalLeakWarning::ERROR ? tr("ERROR") : tr("WARNING"));
        sevItem->setForeground(w.severity == PortalLeakWarning::ERROR ? QColor(255, 75, 75) : QColor(255, 200, 50));
        sevItem->setTextAlignment(Qt::AlignCenter);
        sevItem->setData(Qt::UserRole, origIdx);

        QTableWidgetItem* typeItem = new QTableWidgetItem(w.type);
        
        // Zone column
        QString zoneDisplay = w.zoneName.isEmpty() ? QStringLiteral("—") : w.zoneName;
        QTableWidgetItem* zoneItem = new QTableWidgetItem(zoneDisplay);
        zoneItem->setTextAlignment(Qt::AlignCenter);

        // Coordinates column
        QString coordText = tr("Layer %1 (%2, %3)").arg(w.layer).arg(w.x).arg(w.y);
        if (w.isClash && w.x2 >= 0 && w.y2 >= 0) {
            coordText = tr("L%1 (%2,%3) ↔ (%4,%5)").arg(w.layer).arg(w.x).arg(w.y).arg(w.x2).arg(w.y2);
        }
        QTableWidgetItem* locItem = new QTableWidgetItem(coordText);
        locItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem* descItem = new QTableWidgetItem(w.description);

        m_table->setItem(row, 0, sevItem);
        m_table->setItem(row, 1, typeItem);
        m_table->setItem(row, 2, zoneItem);
        m_table->setItem(row, 3, locItem);
        m_table->setItem(row, 4, descItem);
    }

    QString filterNotice;
    if (m_selectedZoneFilterId != -1) {
        filterNotice = tr(" [Filtered by: %1]").arg(m_cmbZoneFilter->currentText());
    }

    m_lblStats->setText(tr("Showing issues: <b>%1</b> of %2%3 (Physical BSP: %4, Static Grid: %5). Double-click jumps camera to tile.")
                        .arg(m_visibleWarningIndices.size())
                        .arg(m_currentWarnings.size())
                        .arg(filterNotice)
                        .arg(bspCount)
                        .arg(staticCount));

    onTableSelectionChanged();
}

void PortalLeakDialog::onCellDoubleClicked(int row, int /*column*/) {
    if (row >= 0 && row < static_cast<int>(m_visibleWarningIndices.size())) {
        int origIdx = m_visibleWarningIndices[row];
        const auto& w = m_currentWarnings[origIdx];
        emit cellSelected(w.layer, w.x, w.y);
    }
}

void PortalLeakDialog::onTableSelectionChanged() {
    int row = m_table->currentRow();
    if (row >= 0 && row < static_cast<int>(m_visibleWarningIndices.size())) {
        int origIdx = m_visibleWarningIndices[row];
        const auto& w = m_currentWarnings[origIdx];
        m_btnResolve->setEnabled(true);
        if (w.isClash) {
            m_btnResolve->setText(tr("⚡ Resolve Clash..."));
        } else {
            m_btnResolve->setText(tr("🧱 Edit Segment..."));
        }
    } else {
        m_btnResolve->setEnabled(false);
        m_btnResolve->setText(tr("⚡ Resolve Clash / Edit..."));
    }
}

void PortalLeakDialog::onResolveClicked() {
    int row = m_table->currentRow();
    if (row >= 0 && row < static_cast<int>(m_visibleWarningIndices.size())) {
        int origIdx = m_visibleWarningIndices[row];
        const auto& w = m_currentWarnings[origIdx];
        if (w.isClash && w.x2 >= 0 && w.y2 >= 0) {
            emit resolveConflictRequested(w.layer, w.x, w.y, w.x2, w.y2);
        } else {
            emit editSegmentRequested(w.layer, w.x, w.y);
        }
    }
}

void PortalLeakDialog::onTableContextMenu(const QPoint& pos) {
    QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) return;
    int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_visibleWarningIndices.size())) return;

    int origIdx = m_visibleWarningIndices[row];
    const auto& w = m_currentWarnings[origIdx];

    QMenu menu(this);
    QAction* actJump = menu.addAction(tr("🔍 Focus View on Tile (%1, %2)").arg(w.x).arg(w.y));

    menu.addSeparator();
    QAction* actResolve = nullptr;
    QAction* actEditA = nullptr;
    QAction* actEditB = nullptr;

    if (w.isClash && w.x2 >= 0 && w.y2 >= 0) {
        actResolve = menu.addAction(tr("⚡ Resolve Double-Wall Clash..."));
        actEditA = menu.addAction(tr("🧱 Inspect Cell A (%1, %2)").arg(w.x).arg(w.y));
        actEditB = menu.addAction(tr("🧱 Inspect Cell B (%1, %2)").arg(w.x2).arg(w.y2));
    } else {
        actEditA = menu.addAction(tr("🧱 Inspect & Edit Segment (%1, %2)").arg(w.x).arg(w.y));
    }

    QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == actJump) {
        emit cellSelected(w.layer, w.x, w.y);
    } else if (chosen == actResolve) {
        emit resolveConflictRequested(w.layer, w.x, w.y, w.x2, w.y2);
    } else if (chosen == actEditA) {
        emit editSegmentRequested(w.layer, w.x, w.y);
    } else if (chosen == actEditB) {
        emit editSegmentRequested(w.layer, w.x2, w.y2);
    }
}

void PortalLeakDialog::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 — Portal & CSG Leak Detector — %2").arg(VersionInfo::shortTitle(), mapName));

    PortalLeakAnalyzer analyzer(m_map, m_visZoneManager);
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

void PortalLeakDialog::setVisZoneManager(std::shared_ptr<VisZoneManager> mgr) {
    m_visZoneManager = mgr;
    if (m_map) {
        runAnalysis();
    }
}

