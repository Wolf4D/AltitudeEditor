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
#include <QAbstractItemView>
#include <QPainter>

static QIcon makeSourceIcon(bool isPhysical, bool isStatic, bool isMerged) {
    const int S = 20;
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    if (isMerged || (isPhysical && isStatic)) {
        // BOTH AT ONCE: Composite dual badge (2D Grid + 3D Cube + Verified Badge)
        // 1. Draw small 2D grid at bottom-left
        p.setPen(QPen(QColor(245, 158, 11), 1.2));
        p.setBrush(QColor(245, 158, 11, 40));
        p.drawRoundedRect(QRectF(1.5, 7, 9, 9), 1.2, 1.2);
        p.drawLine(QPointF(6, 7), QPointF(6, 16));
        p.drawLine(QPointF(1.5, 11.5), QPointF(10.5, 11.5));

        // 2. Draw 3D Cube at top-right
        QPolygonF topFace;
        topFace << QPointF(14.5, 1.5) << QPointF(19, 3.8) << QPointF(14.5, 6) << QPointF(10, 3.8);
        p.setPen(QPen(QColor(14, 165, 233), 1.2));
        p.setBrush(QColor(56, 189, 248, 160));
        p.drawPolygon(topFace);

        QPolygonF leftFace;
        leftFace << QPointF(10, 3.8) << QPointF(14.5, 6) << QPointF(14.5, 11.5) << QPointF(10, 9.2);
        p.setBrush(QColor(2, 132, 199, 180));
        p.drawPolygon(leftFace);

        QPolygonF rightFace;
        rightFace << QPointF(14.5, 6) << QPointF(19, 3.8) << QPointF(19, 9.2) << QPointF(14.5, 11.5);
        p.setBrush(QColor(3, 105, 161, 200));
        p.drawPolygon(rightFace);

        // 3. Small emerald confirmation check badge at bottom-right
        p.setPen(QPen(QColor(16, 185, 129), 1.0));
        p.setBrush(QColor(16, 185, 129));
        p.drawEllipse(QRectF(11, 11, 8, 8));
        p.setPen(QPen(Qt::white, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(12.8, 15), QPointF(14.5, 16.8));
        p.drawLine(QPointF(14.5, 16.8), QPointF(17.5, 13.2));
    } else if (isPhysical) {
        // PHYSICAL ONLY: 3D Isometric Polyhedron Cube (Compiled BSP)
        QPolygonF topFace;
        topFace << QPointF(10, 2) << QPointF(17.5, 5.8) << QPointF(10, 9.5) << QPointF(2.5, 5.8);
        p.setPen(QPen(QColor(14, 165, 233), 1.3));
        p.setBrush(QColor(56, 189, 248, 140));
        p.drawPolygon(topFace);

        QPolygonF leftFace;
        leftFace << QPointF(2.5, 5.8) << QPointF(10, 9.5) << QPointF(10, 18) << QPointF(2.5, 14.2);
        p.setBrush(QColor(2, 132, 199, 180));
        p.drawPolygon(leftFace);

        QPolygonF rightFace;
        rightFace << QPointF(10, 9.5) << QPointF(17.5, 5.8) << QPointF(17.5, 14.2) << QPointF(10, 18);
        p.setBrush(QColor(3, 105, 161, 220));
        p.drawPolygon(rightFace);

        // Subtle center line
        p.setPen(QPen(QColor(224, 242, 254), 1.0));
        p.drawLine(QPointF(10, 9.5), QPointF(10, 18));
    } else {
        // LOGICAL ONLY: 2D Grid / Blueprint (.FPM)
        p.setPen(QPen(QColor(245, 158, 11), 1.4));
        p.setBrush(QColor(245, 158, 11, 35));
        p.drawRoundedRect(QRectF(2.5, 2.5, 15, 15), 2.0, 2.0);

        p.setPen(QPen(QColor(217, 119, 6), 1.1));
        // Cross lines
        p.drawLine(QPointF(10, 2.5), QPointF(10, 17.5));
        p.drawLine(QPointF(2.5, 10), QPointF(17.5, 10));

        // Center dot
        p.setBrush(QColor(245, 158, 11));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(8.5, 8.5, 3, 3));
    }

    p.end();
    return QIcon(pix);
}

PortalLeakDialog::PortalLeakDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr, QWidget* parent)
    : QDialog(parent), m_map(map), m_visZoneManager(visZoneMgr)
{
    updateDialogTitle();
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
    m_cmbZoneFilter->setMaxVisibleItems(15);
    m_cmbZoneFilter->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_cmbZoneFilter->setStyleSheet(QStringLiteral("QComboBox { combobox-popup: 0; background-color: #1e2630; color: #cad8e6; border: 1px solid #3d4f61; border-radius: 4px; padding: 5px 8px; }"
                                                 "QComboBox QAbstractItemView { max-height: 280px; background-color: #1e2630; color: #cad8e6; selection-background-color: #2e537a; }"));
    ctrlLayout->addWidget(m_cmbZoneFilter);

    ctrlLayout->addSpacing(10);
    m_btnResolve = new QPushButton(tr("⚡ Resolve Clash / Edit..."), this);
    m_btnResolve->setEnabled(false);
    m_btnResolve->setToolTip(tr("Open the Segment Editor & Conflict Resolver for the selected row"));
    m_btnResolve->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #243d26; color: #72f07b; border: 1px solid #3b6b3e; border-radius: 4px; padding: 7px 14px; }"
                                               "QPushButton:hover { background-color: #2f5432; color: #94ff9c; }"
                                               "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; border-color: #2b3846; }"));
    ctrlLayout->addWidget(m_btnResolve);

    ctrlLayout->addSpacing(6);
    m_btnSuppress = new QPushButton(tr("🚫 Suppress"), this);
    m_btnSuppress->setEnabled(false);
    m_btnSuppress->setToolTip(tr("Suppress this warning (pack into .FPM) [M]"));
    m_btnSuppress->setShortcut(QKeySequence(Qt::Key_M));
    m_btnSuppress->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #3b2a2a; color: #f87171; border: 1px solid #6b3b3b; border-radius: 4px; padding: 7px 14px; }"
                                               "QPushButton:hover { background-color: #4a3434; color: #fca5a5; }"
                                               "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; border-color: #2b3846; }"));
    ctrlLayout->addWidget(m_btnSuppress);

    ctrlLayout->addSpacing(10);
    m_chkShowSuppressed = new QCheckBox(tr("Show Suppressed (0)"), this);
    m_chkShowSuppressed->setStyleSheet(QStringLiteral("QCheckBox { color: #94a3b8; font-size: 12px; } QCheckBox:hover { color: #cbd5e1; }"));
    ctrlLayout->addWidget(m_chkShowSuppressed);

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
    m_table->setIconSize(QSize(20, 20));
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
    connect(m_btnSuppress, &QPushButton::clicked, this, &PortalLeakDialog::onSuppressClicked);
    connect(m_chkShowSuppressed, &QCheckBox::toggled, this, &PortalLeakDialog::onShowSuppressedToggled);

    // Load suppressions from map container
    m_suppressionMgr.loadFromMap(m_map);

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
    updateDialogTitle();

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
    if (m_chkShowSuppressed) {
        m_chkShowSuppressed->setText(tr("Show Suppressed (%1)").arg(m_suppressionMgr.suppressedCount()));
    }
    if (m_btnSuppress) {
        onTableSelectionChanged();
    }

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
        emit warningsUpdated(m_currentWarnings);
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
    
    std::vector<PortalLeakWarning> activeWarnings;
    for (auto& w : m_currentWarnings) {
        w.isSuppressed = m_suppressionMgr.isSuppressed(w);
        if (!w.isSuppressed) {
            activeWarnings.push_back(w);
        }
    }

    QApplication::restoreOverrideCursor();

    populateZoneFilter();
    updateTableRows();
    emit warningsUpdated(activeWarnings);
}

void PortalLeakDialog::populateZoneFilter() {
    int prevFilterId = m_selectedZoneFilterId;

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

    if (unassignedCount > 0 || prevFilterId == -2) {
        if (unassignedCount == 0) {
            m_cmbZoneFilter->addItem(tr("Outside / Void / Unzoned (0 — Resolved)"), -2);
        } else {
            m_cmbZoneFilter->addItem(tr("Outside / Void / Unzoned (%1)").arg(unassignedCount), -2);
        }
    }

    // Collect all zones that have issues (>0), OR that were previously selected by user
    QSet<int> zidsToShow;
    for (auto it = zoneCounts.constBegin(); it != zoneCounts.constEnd(); ++it) {
        if (it.value() > 0) {
            zidsToShow.insert(it.key());
        }
    }
    if (prevFilterId >= 0) {
        zidsToShow.insert(prevFilterId);
    }

    QList<int> sortedZones = zidsToShow.values();
    std::sort(sortedZones.begin(), sortedZones.end());

    for (int zid : sortedZones) {
        int count = zoneCounts.value(zid, 0);
        QString zName = QString("Zone %1").arg(zid + 1);
        if (m_visZoneManager) {
            const VisZone* vz = m_visZoneManager->getZone(zid);
            if (vz) {
                zName = vz->name;
            }
        }
        if (count == 0) {
            m_cmbZoneFilter->addItem(tr("%1 — 0 issues (Resolved)").arg(zName), zid);
        } else {
            m_cmbZoneFilter->addItem(tr("%1 — %2 issues").arg(zName).arg(count), zid);
        }
    }

    // Restore selected filter if still present
    int selectIdx = 0;
    for (int i = 0; i < m_cmbZoneFilter->count(); ++i) {
        if (m_cmbZoneFilter->itemData(i).toInt() == prevFilterId) {
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
    int prevSelectedRow = m_table->currentRow();
    m_visibleWarningIndices.clear();

    int totalSuppressed = 0;
    for (const auto& w : m_currentWarnings) {
        if (w.isSuppressed) {
            totalSuppressed++;
        }
    }
    m_chkShowSuppressed->setText(tr("Show Suppressed (%1)").arg(totalSuppressed));

    int mergedCount = 0;
    int bspCount = 0;
    int staticCount = 0;

    for (int i = 0; i < static_cast<int>(m_currentWarnings.size()); ++i) {
        const auto& w = m_currentWarnings[i];

        if (!m_showSuppressed && w.isSuppressed) {
            continue;
        }

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
            if (w.isMerged) {
                mergedCount++;
            } else if (w.isPhysicalBsp) {
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

        QString sevText;
        if (w.isSuppressed) {
            sevText = tr("[SUPPRESSED]");
        } else {
            sevText = (w.severity == PortalLeakWarning::ERROR) ? tr("ERROR") : tr("WARNING");
        }
        QTableWidgetItem* sevItem = new QTableWidgetItem(sevText);
        if (w.isSuppressed) {
            sevItem->setForeground(QColor(148, 163, 184));
        } else {
            sevItem->setForeground(w.severity == PortalLeakWarning::ERROR ? QColor(255, 75, 75) : QColor(255, 200, 50));
        }
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

        // Issue Description column (includes source verification icon, estimated size tag, and detailed tooltip)
        QString displayDesc = w.description;
        if (w.hasPhysicalSize) {
            QString sizeTag;
            if (w.portalWidth < 95.0f || w.portalHeight < 95.0f) {
                sizeTag = tr("[%1×%2 (seam)] ").arg(w.portalWidth, 0, 'f', 0).arg(w.portalHeight, 0, 'f', 0);
            } else {
                sizeTag = tr("[%1×%2] ").arg(w.portalWidth, 0, 'f', 0).arg(w.portalHeight, 0, 'f', 0);
            }
            displayDesc = sizeTag + displayDesc;
        }

        QTableWidgetItem* descItem = new QTableWidgetItem(displayDesc);
        descItem->setIcon(makeSourceIcon(w.isPhysicalBsp, w.isStaticMap, w.isMerged));

        QString sizeInfo;
        if (w.hasPhysicalSize) {
            float mWidth = w.portalWidth * 0.0254f;
            float mHeight = w.portalHeight * 0.0254f;
            sizeInfo = tr("\nEstimated Portal Size: %1 × %2 (~%3 × %4 m)")
                           .arg(w.portalWidth, 0, 'f', 0)
                           .arg(w.portalHeight, 0, 'f', 0)
                           .arg(mWidth, 0, 'f', 1)
                           .arg(mHeight, 0, 'f', 1);
        }

        QString sourceTooltip;
        if (w.isSuppressed) {
            sourceTooltip = tr("[STATUS: SUPPRESSED]\n");
        }
        if (w.isMerged) {
            sourceTooltip += tr("[Source: Both (Physical BSP + Logical Grid)]%1\n%2").arg(sizeInfo, w.description);
        } else if (w.isPhysicalBsp) {
            sourceTooltip += tr("[Source: Physical BSP (universe.dbu)]%1\n%2").arg(sizeInfo, w.description);
        } else {
            sourceTooltip += tr("[Source: Logical Grid (.FPM)]\n%1").arg(w.description);
        }
        descItem->setToolTip(sourceTooltip);

        if (w.isSuppressed) {
            QFont italicFont = m_table->font();
            italicFont.setItalic(true);
            QColor mutedColor(148, 163, 184);

            sevItem->setFont(italicFont);
            typeItem->setFont(italicFont);
            typeItem->setForeground(mutedColor);
            zoneItem->setFont(italicFont);
            zoneItem->setForeground(mutedColor);
            locItem->setFont(italicFont);
            locItem->setForeground(mutedColor);
            descItem->setFont(italicFont);
            descItem->setForeground(mutedColor);
        }

        m_table->setItem(row, 0, sevItem);
        m_table->setItem(row, 1, typeItem);
        m_table->setItem(row, 2, zoneItem);
        m_table->setItem(row, 3, locItem);
        m_table->setItem(row, 4, descItem);
    }

    if (m_table->rowCount() > 0) {
        int newRow = qBound(0, prevSelectedRow >= 0 ? prevSelectedRow : 0, m_table->rowCount() - 1);
        m_table->selectRow(newRow);
    }

    QString filterNotice;
    if (m_selectedZoneFilterId != -1) {
        filterNotice = tr(" [Filtered by: %1]").arg(m_cmbZoneFilter->currentText());
    }

    QString suppressedNotice;
    if (totalSuppressed > 0) {
        suppressedNotice = tr(" (Suppressed: %1)").arg(totalSuppressed);
    }

    if (mergedCount > 0) {
        m_lblStats->setText(tr("Showing issues: <b>%1</b> of %2%3%4 (Both: %5, Physical: %6, Logical: %7). Double-click jumps camera to tile.")
                            .arg(m_visibleWarningIndices.size())
                            .arg(m_currentWarnings.size())
                            .arg(filterNotice)
                            .arg(suppressedNotice)
                            .arg(mergedCount)
                            .arg(bspCount)
                            .arg(staticCount));
    } else {
        m_lblStats->setText(tr("Showing issues: <b>%1</b> of %2%3%4 (Physical: %5, Logical: %6). Double-click jumps camera to tile.")
                            .arg(m_visibleWarningIndices.size())
                            .arg(m_currentWarnings.size())
                            .arg(filterNotice)
                            .arg(suppressedNotice)
                            .arg(bspCount)
                            .arg(staticCount));
    }

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

        m_btnSuppress->setEnabled(true);
        if (w.isSuppressed) {
            m_btnSuppress->setText(tr("↩️ Unsuppress"));
            m_btnSuppress->setToolTip(tr("Restore this warning to active state [M]"));
            m_btnSuppress->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #243d26; color: #72f07b; border: 1px solid #3b6b3e; border-radius: 4px; padding: 7px 14px; }"
                                                       "QPushButton:hover { background-color: #2f5432; color: #94ff9c; }"
                                                       "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; border-color: #2b3846; }"));
        } else {
            m_btnSuppress->setText(tr("🚫 Suppress"));
            m_btnSuppress->setToolTip(tr("Suppress this warning (pack into .FPM) [M]"));
            m_btnSuppress->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; background-color: #3b2a2a; color: #f87171; border: 1px solid #6b3b3b; border-radius: 4px; padding: 7px 14px; }"
                                                       "QPushButton:hover { background-color: #4a3434; color: #fca5a5; }"
                                                       "QPushButton:disabled { background-color: #1c242d; color: #5a6e82; border-color: #2b3846; }"));
        }

        emit cellSelected(w.layer, w.x, w.y);
    } else {
        m_btnResolve->setEnabled(false);
        m_btnResolve->setText(tr("⚡ Resolve Clash / Edit..."));
        m_btnSuppress->setEnabled(false);
        m_btnSuppress->setText(tr("🚫 Suppress"));
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

void PortalLeakDialog::updateDialogTitle() {
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    if (m_map && m_map->isModified) {
        mapName += QStringLiteral("*");
    }
    setWindowTitle(tr("%1 — Portal & CSG Leak Detector — %2").arg(VersionInfo::shortTitle(), mapName));
}

void PortalLeakDialog::onSuppressClicked() {
    int row = m_table->currentRow();
    if (row >= 0 && row < static_cast<int>(m_visibleWarningIndices.size())) {
        int origIdx = m_visibleWarningIndices[row];
        auto& w = m_currentWarnings[origIdx];
        if (w.isSuppressed) {
            m_suppressionMgr.unsuppress(w);
            w.isSuppressed = false;
        } else {
            m_suppressionMgr.suppress(w);
            w.isSuppressed = true;
        }
        m_suppressionMgr.saveToMap(m_map, false);
        if (m_map) {
            m_map->isModified = true;
        }
        updateDialogTitle();
        emit mapModified();

        std::vector<PortalLeakWarning> activeWarnings;
        for (const auto& item : m_currentWarnings) {
            if (!item.isSuppressed) {
                activeWarnings.push_back(item);
            }
        }
        emit warningsUpdated(activeWarnings);

        updateTableRows();
    }
}

void PortalLeakDialog::onShowSuppressedToggled(bool checked) {
    m_showSuppressed = checked;
    updateTableRows();
}

void PortalLeakDialog::onUnsuppressAllClicked() {
    if (m_suppressionMgr.suppressedCount() == 0) return;
    int count = m_suppressionMgr.suppressedCount();
    auto reply = QMessageBox::question(this, tr("Restore All Warnings"),
                                       tr("Restore all %1 suppressed warnings back to active state?").arg(count),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply == QMessageBox::Yes) {
        m_suppressionMgr.unsuppressAll();
        m_suppressionMgr.saveToMap(m_map, false);
        if (m_map) {
            m_map->isModified = true;
        }
        updateDialogTitle();
        emit mapModified();

        for (auto& w : m_currentWarnings) {
            w.isSuppressed = false;
        }
        std::vector<PortalLeakWarning> activeWarnings = m_currentWarnings;
        emit warningsUpdated(activeWarnings);
        updateTableRows();
    }
}

void PortalLeakDialog::onTableContextMenu(const QPoint& pos) {
    QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        if (m_suppressionMgr.suppressedCount() > 0) {
            QMenu menu(this);
            QAction* actRestoreAll = menu.addAction(tr("↩️ Restore All Suppressed Warnings (%1)...").arg(m_suppressionMgr.suppressedCount()));
            QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
            if (chosen == actRestoreAll) {
                onUnsuppressAllClicked();
            }
        }
        return;
    }

    int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_visibleWarningIndices.size())) return;

    if (m_table->currentRow() != row) {
        m_table->selectRow(row);
    }

    int origIdx = m_visibleWarningIndices[row];
    const auto& w = m_currentWarnings[origIdx];

    QMenu menu(this);

    // 1. Suppress / Restore Warning at the VERY TOP of the context menu
    QAction* actToggleSuppress = nullptr;
    if (w.isSuppressed) {
        actToggleSuppress = menu.addAction(tr("↩️ Restore Warning"));
    } else {
        actToggleSuppress = menu.addAction(tr("🚫 Suppress Warning"));
    }
    QFont boldFont = actToggleSuppress->font();
    boldFont.setBold(true);
    actToggleSuppress->setFont(boldFont);

    QAction* actRestoreAll = nullptr;
    if (m_suppressionMgr.suppressedCount() > 0) {
        actRestoreAll = menu.addAction(tr("↩️ Restore All Suppressed (%1)...").arg(m_suppressionMgr.suppressedCount()));
    }

    menu.addSeparator();

    // 2. Camera Navigation
    QAction* actJump = menu.addAction(tr("🔍 Focus View on Tile (%1, %2)").arg(w.x).arg(w.y));

    menu.addSeparator();

    // 3. Segment Editing / Resolving
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
    if (chosen == actToggleSuppress) {
        onSuppressClicked();
    } else if (chosen == actRestoreAll) {
        onUnsuppressAllClicked();
    } else if (chosen == actJump) {
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
    m_suppressionMgr.loadFromMap(m_map);

    updateDialogTitle();

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

