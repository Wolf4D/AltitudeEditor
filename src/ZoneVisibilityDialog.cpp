#include "ZoneVisibilityDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QMessageBox>
#include <QEvent>
#include <QCloseEvent>
#include <QScrollBar>

ZoneVisibilityDialog::ZoneVisibilityDialog(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneMgr, int initialZoneId, QWidget* parent)
    : QDialog(parent), m_map(map), m_visZoneManager(visZoneMgr), m_analyzer(map, visZoneMgr), m_currentOriginZoneId(initialZoneId)
{
    setupUi();
    populateOriginZones();
    refreshGraph();
}

void ZoneVisibilityDialog::setupUi() {
    setWindowTitle(tr("👁️ VisZone Visibility Tracer (PVS Graph)"));
    resize(1080, 700);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #111827; color: #f3f4f6; }"
        "QTableWidget { background-color: #0f172a; alternate-background-color: #1e293b; color: #f1f5f9; gridline-color: #334155; border: 1px solid #334155; border-radius: 6px; font-size: 12px; }"
        "QTableWidget::item:selected { background-color: #2563eb; color: #ffffff; }"
        "QHeaderView::section { background-color: #1e293b; color: #94a3b8; font-weight: bold; padding: 6px; border: 1px solid #334155; }"
        "QComboBox { background-color: #1e293b; color: #f1f5f9; border: 1px solid #475569; border-radius: 6px; padding: 5px 10px; font-size: 13px; font-weight: 500; }"
        "QComboBox QAbstractItemView { background-color: #0f172a; color: #f1f5f9; selection-background-color: #2563eb; }"
        "QLineEdit { background-color: #0f172a; color: #f1f5f9; border: 1px solid #475569; border-radius: 6px; padding: 6px 10px; font-size: 12px; }"
        "QPushButton { background-color: #1e293b; color: #cbd5e1; border: 1px solid #475569; border-radius: 6px; padding: 6px 14px; font-weight: 500; font-size: 12px; }"
        "QPushButton:hover { background-color: #334155; color: #ffffff; }"
        "QPushButton:disabled { background-color: #0f172a; color: #475569; border-color: #1e293b; }"
        "QSplitter::handle { background-color: #334155; width: 4px; }"
        "QScrollArea { border: none; background-color: transparent; }"
    ));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // ==========================================
    // 1. TOP BAR: Origin Zone & Global Actions
    // ==========================================
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->setSpacing(10);

    QLabel* lblOrigin = new QLabel(tr("📍 Origin Room (Camera / Player View):"), this);
    lblOrigin->setStyleSheet(QStringLiteral("font-weight: bold; color: #93c5fd; font-size: 13px;"));
    topBar->addWidget(lblOrigin);

    m_cmbOriginZone = new QComboBox(this);
    m_cmbOriginZone->setMinimumWidth(320);
    topBar->addWidget(m_cmbOriginZone, 1);

    m_btnFocusOrigin = new QPushButton(tr("🔍 Focus Room on Map"), this);
    m_btnFocusOrigin->setToolTip(tr("Center map camera on this origin room"));
    topBar->addWidget(m_btnFocusOrigin);

    m_btnRefresh = new QPushButton(tr("🔄 Recompute PVS"), this);
    m_btnRefresh->setToolTip(tr("Rebuild portal reachability graph from universe.dbu"));
    topBar->addWidget(m_btnRefresh);

    mainLayout->addLayout(topBar);

    // ==========================================
    // 2. KPI SUMMARY BADGES BAR
    // ==========================================
    QHBoxLayout* kpiLayout = new QHBoxLayout();
    kpiLayout->setSpacing(10);

    // Badge 1: Status
    m_badgeStatusFrame = new QFrame(this);
    m_badgeStatusFrame->setFrameShape(QFrame::StyledPanel);
    QHBoxLayout* b1Layout = new QHBoxLayout(m_badgeStatusFrame);
    b1Layout->setContentsMargins(10, 6, 10, 6);
    m_lblBadgeStatus = new QLabel(m_badgeStatusFrame);
    m_lblBadgeStatus->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
    b1Layout->addWidget(m_lblBadgeStatus);
    kpiLayout->addWidget(m_badgeStatusFrame, 2);

    // Badge 2: Total Visible
    m_badgeTotalFrame = new QFrame(this);
    m_badgeTotalFrame->setFrameShape(QFrame::StyledPanel);
    m_badgeTotalFrame->setStyleSheet(QStringLiteral("QFrame { background-color: #1e293b; border: 1px solid #334155; border-radius: 6px; }"));
    QHBoxLayout* b2Layout = new QHBoxLayout(m_badgeTotalFrame);
    b2Layout->setContentsMargins(10, 6, 10, 6);
    m_lblBadgeTotal = new QLabel(m_badgeTotalFrame);
    m_lblBadgeTotal->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 12px;"));
    b2Layout->addWidget(m_lblBadgeTotal);
    kpiLayout->addWidget(m_badgeTotalFrame, 1);

    // Badge 3: Doorways LOS
    m_badgeDoorsFrame = new QFrame(this);
    m_badgeDoorsFrame->setFrameShape(QFrame::StyledPanel);
    m_badgeDoorsFrame->setStyleSheet(QStringLiteral("QFrame { background-color: #1e293b; border: 1px solid #334155; border-radius: 6px; }"));
    QHBoxLayout* b3Layout = new QHBoxLayout(m_badgeDoorsFrame);
    b3Layout->setContentsMargins(10, 6, 10, 6);
    m_lblBadgeDoors = new QLabel(m_badgeDoorsFrame);
    m_lblBadgeDoors->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 12px;"));
    b3Layout->addWidget(m_lblBadgeDoors);
    kpiLayout->addWidget(m_badgeDoorsFrame, 1);

    mainLayout->addLayout(kpiLayout);

    // ==========================================
    // 3. MASTER-DETAIL SPLITTER
    // ==========================================
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // ------------------------------------------
    // LEFT PANE: Filter Tabs & Target Zone List
    // ------------------------------------------
    QWidget* leftWidget = new QWidget(splitter);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    // Filter Category Tabs
    QHBoxLayout* tabsLayout = new QHBoxLayout();
    tabsLayout->setSpacing(4);

    m_btnTabBreaches = new QPushButton(tr("🚨 Leaks (0)"), leftWidget);
    m_btnTabDoors = new QPushButton(tr("🚪 Doors (0)"), leftWidget);
    m_btnTabAll = new QPushButton(tr("🌐 All (0)"), leftWidget);

    tabsLayout->addWidget(m_btnTabBreaches);
    tabsLayout->addWidget(m_btnTabDoors);
    tabsLayout->addWidget(m_btnTabAll);
    leftLayout->addLayout(tabsLayout);

    // Search Box
    m_searchFilter = new QLineEdit(leftWidget);
    m_searchFilter->setPlaceholderText(tr("🔍 Filter by room name or ID..."));
    m_searchFilter->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchFilter);

    // Target Zones Table
    m_tableZones = new QTableWidget(leftWidget);
    m_tableZones->setColumnCount(3);
    m_tableZones->setHorizontalHeaderLabels({
        tr("Target Room"),
        tr("Visibility Cause"),
        tr("Hops")
    });
    m_tableZones->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tableZones->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableZones->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tableZones->setColumnWidth(0, 110);
    m_tableZones->setColumnWidth(2, 70);
    m_tableZones->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableZones->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableZones->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableZones->setAlternatingRowColors(true);
    leftLayout->addWidget(m_tableZones);

    splitter->addWidget(leftWidget);

    // ------------------------------------------
    // RIGHT PANE: Visual Causal Chain Inspector
    // ------------------------------------------
    QWidget* rightWidget = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(6, 0, 0, 0);
    rightLayout->setSpacing(8);

    // Header Title
    QHBoxLayout* chainTitleLayout = new QHBoxLayout();
    m_lblChainTitle = new QLabel(tr("Select a target room from the list to inspect visibility traversal"), rightWidget);
    m_lblChainTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #f1f5f9;"));
    chainTitleLayout->addWidget(m_lblChainTitle);

    m_lblChainSubtitle = new QLabel(rightWidget);
    m_lblChainSubtitle->setStyleSheet(QStringLiteral("font-size: 12px; color: #94a3b8; font-weight: 500;"));
    chainTitleLayout->addWidget(m_lblChainSubtitle);
    chainTitleLayout->addStretch();
    rightLayout->addLayout(chainTitleLayout);

    // Diagnosis Banner Box
    m_frameDiagnosis = new QFrame(rightWidget);
    m_frameDiagnosis->setFrameShape(QFrame::StyledPanel);
    QHBoxLayout* diagLayout = new QHBoxLayout(m_frameDiagnosis);
    diagLayout->setContentsMargins(10, 8, 10, 8);
    diagLayout->setSpacing(10);

    m_lblDiagnosisIcon = new QLabel(m_frameDiagnosis);
    m_lblDiagnosisIcon->setStyleSheet(QStringLiteral("font-size: 20px;"));
    diagLayout->addWidget(m_lblDiagnosisIcon);

    m_lblDiagnosisText = new QLabel(m_frameDiagnosis);
    m_lblDiagnosisText->setWordWrap(true);
    m_lblDiagnosisText->setStyleSheet(QStringLiteral("font-size: 12px;"));
    diagLayout->addWidget(m_lblDiagnosisText, 1);
    rightLayout->addWidget(m_frameDiagnosis);

    // Scrollable Visual Chain Flow Cards
    m_scrollChain = new QScrollArea(rightWidget);
    m_scrollChain->setWidgetResizable(true);
    m_scrollChain->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_chainWidget = new QWidget();
    m_chainLayout = new QVBoxLayout(m_chainWidget);
    m_chainLayout->setContentsMargins(4, 4, 4, 4);
    m_chainLayout->setSpacing(6);
    m_chainLayout->addStretch();
    m_scrollChain->setWidget(m_chainWidget);

    rightLayout->addWidget(m_scrollChain, 1);

    splitter->addWidget(rightWidget);

    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 6);
    splitter->setSizes({380, 700});
    mainLayout->addWidget(splitter, 1);

    // ==========================================
    // 4. BOTTOM ACTION BAR
    // ==========================================
    QHBoxLayout* bottomBar = new QHBoxLayout();
    bottomBar->setSpacing(10);

    m_btnJumpToCulprit = new QPushButton(tr("🔍 Jump to Culprit Breach on Map"), this);
    m_btnJumpToCulprit->setEnabled(false);
    m_btnJumpToCulprit->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; background-color: #991b1b; color: #fecaca; border: 1px solid #ef4444; border-radius: 6px; padding: 8px 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: #b91c1c; color: #ffffff; }"
        "QPushButton:disabled { background-color: #1e293b; color: #475569; border-color: #334155; }"
    ));
    bottomBar->addWidget(m_btnJumpToCulprit);

    m_btnEditCulprit = new QPushButton(tr("🧱 Edit Wall / Slab at Breach..."), this);
    m_btnEditCulprit->setEnabled(false);
    m_btnEditCulprit->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; background-color: #78350f; color: #fde68a; border: 1px solid #f59e0b; border-radius: 6px; padding: 8px 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: #92400e; color: #ffffff; }"
        "QPushButton:disabled { background-color: #1e293b; color: #475569; border-color: #334155; }"
    ));
    bottomBar->addWidget(m_btnEditCulprit);

    bottomBar->addStretch();

    m_btnClose = new QPushButton(tr("Close (Esc)"), this);
    m_btnClose->setShortcut(QKeySequence(Qt::Key_Escape));
    bottomBar->addWidget(m_btnClose);

    mainLayout->addLayout(bottomBar);

    // ==========================================
    // 5. SIGNALS & SLOTS
    // ==========================================
    connect(m_cmbOriginZone, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ZoneVisibilityDialog::onOriginZoneChanged);
    connect(m_btnRefresh, &QPushButton::clicked, this, &ZoneVisibilityDialog::refreshGraph);
    connect(m_btnFocusOrigin, &QPushButton::clicked, this, &ZoneVisibilityDialog::onFocusOriginZoneClicked);

    connect(m_btnTabBreaches, &QPushButton::clicked, this, &ZoneVisibilityDialog::onTabBreachesClicked);
    connect(m_btnTabDoors, &QPushButton::clicked, this, &ZoneVisibilityDialog::onTabDoorsClicked);
    connect(m_btnTabAll, &QPushButton::clicked, this, &ZoneVisibilityDialog::onTabAllClicked);
    connect(m_searchFilter, &QLineEdit::textChanged, this, &ZoneVisibilityDialog::onSearchTextChanged);

    connect(m_tableZones, &QTableWidget::itemSelectionChanged, this, &ZoneVisibilityDialog::onZoneSelectionChanged);
    connect(m_tableZones, &QTableWidget::cellDoubleClicked, this, &ZoneVisibilityDialog::onZoneTableDoubleClicked);

    connect(m_btnJumpToCulprit, &QPushButton::clicked, this, &ZoneVisibilityDialog::onJumpToCulpritClicked);
    connect(m_btnEditCulprit, &QPushButton::clicked, this, &ZoneVisibilityDialog::onEditWallAtCulpritClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void ZoneVisibilityDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void ZoneVisibilityDialog::closeEvent(QCloseEvent* event) {
    emit tracePathSelected({});
    QDialog::closeEvent(event);
}

void ZoneVisibilityDialog::retranslateUi() {
    setWindowTitle(tr("👁️ VisZone Visibility Tracer (PVS Graph)"));
    updateKpiBadges();
    updateTabButtons();
    updateZoneList();
}

void ZoneVisibilityDialog::populateOriginZones() {
    m_cmbOriginZone->blockSignals(true);
    m_cmbOriginZone->clear();

    if (m_visZoneManager) {
        const auto& zones = m_visZoneManager->zones();
        for (size_t i = 0; i < zones.size(); ++i) {
            const auto& z = zones[i];
            QString title;
            if (!z.name.isEmpty()) {
                title = QString("%1: %2 (Floor %3-%4, %5 tiles)").arg(z.id + 1).arg(z.name).arg(z.minFloor).arg(z.maxFloor).arg(z.tiles.size());
            } else {
                title = QString("Zone %1 (Floor %2-%3, %4 tiles)").arg(z.id + 1).arg(z.minFloor).arg(z.maxFloor).arg(z.tiles.size());
            }
            m_cmbOriginZone->addItem(title, z.id);
        }
    }

    int idx = m_cmbOriginZone->findData(m_currentOriginZoneId);
    if (idx >= 0) {
        m_cmbOriginZone->setCurrentIndex(idx);
    } else if (m_cmbOriginZone->count() > 0) {
        m_cmbOriginZone->setCurrentIndex(0);
        m_currentOriginZoneId = m_cmbOriginZone->currentData().toInt();
    }
    m_cmbOriginZone->blockSignals(false);
}

void ZoneVisibilityDialog::setOriginZone(int zoneId) {
    m_currentOriginZoneId = zoneId;
    int idx = m_cmbOriginZone->findData(zoneId);
    if (idx >= 0) {
        m_cmbOriginZone->setCurrentIndex(idx);
    }
}

void ZoneVisibilityDialog::refreshGraph() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_pvsGraph = m_analyzer.buildPvsGraph();
    QApplication::restoreOverrideCursor();

    updateKpiBadges();
    updateTabButtons();
    updateZoneList();
}

void ZoneVisibilityDialog::onOriginZoneChanged(int index) {
    if (index >= 0) {
        m_currentOriginZoneId = m_cmbOriginZone->itemData(index).toInt();
        updateKpiBadges();
        updateTabButtons();
        updateZoneList();
    }
}

void ZoneVisibilityDialog::onFocusOriginZoneClicked() {
    if (!m_visZoneManager) return;
    const VisZone* z = m_visZoneManager->getZone(m_currentOriginZoneId);
    if (z && !z->tiles.empty()) {
        const QPoint& pt = z->tiles.front();
        emit cellSelected(z->minFloor, pt.x(), pt.y());
    }
}

void ZoneVisibilityDialog::onTabBreachesClicked() {
    m_activeTab = FilterTab::BreachesOnly;
    updateTabButtons();
    updateZoneList();
}

void ZoneVisibilityDialog::onTabDoorsClicked() {
    m_activeTab = FilterTab::DoorsOnly;
    updateTabButtons();
    updateZoneList();
}

void ZoneVisibilityDialog::onTabAllClicked() {
    m_activeTab = FilterTab::AllZones;
    updateTabButtons();
    updateZoneList();
}

void ZoneVisibilityDialog::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    updateZoneList();
}

void ZoneVisibilityDialog::updateTabButtons() {
    int breachCount = 0;
    int doorCount = 0;
    int totalCount = 0;

    if (m_pvsGraph.contains(m_currentOriginZoneId)) {
        const auto& pvsInfo = m_pvsGraph[m_currentOriginZoneId];
        totalCount = static_cast<int>(pvsInfo.pathsToOtherZones.size());
        for (auto it = pvsInfo.pathsToOtherZones.begin(); it != pvsInfo.pathsToOtherZones.end(); ++it) {
            bool hasBreach = false;
            for (const auto& conn : it.value()) {
                if (conn.isBreach || conn.isCrack) {
                    hasBreach = true;
                    break;
                }
            }
            if (hasBreach) breachCount++;
            else doorCount++;
        }
    }

    m_btnTabBreaches->setText(tr("🚨 Leaks (%1)").arg(breachCount));
    m_btnTabDoors->setText(tr("🚪 Doors (%1)").arg(doorCount));
    m_btnTabAll->setText(tr("🌐 All (%1)").arg(totalCount));

    auto setTabStyle = [](QPushButton* btn, bool active, const QString& activeBg, const QString& activeColor, const QString& activeBorder) {
        if (active) {
            btn->setStyleSheet(QString("QPushButton { font-weight: bold; background-color: %1; color: %2; border: 2px solid %3; border-radius: 6px; padding: 6px 12px; }")
                               .arg(activeBg, activeColor, activeBorder));
        } else {
            btn->setStyleSheet(QStringLiteral("QPushButton { font-weight: normal; background-color: #1e293b; color: #94a3b8; border: 1px solid #334155; border-radius: 6px; padding: 6px 12px; }"
                                              "QPushButton:hover { background-color: #334155; color: #ffffff; }"));
        }
    };

    setTabStyle(m_btnTabBreaches, m_activeTab == FilterTab::BreachesOnly, "#451a1a", "#fca5a5", "#ef4444");
    setTabStyle(m_btnTabDoors, m_activeTab == FilterTab::DoorsOnly, "#1e3a5f", "#93c5fd", "#3b82f6");
    setTabStyle(m_btnTabAll, m_activeTab == FilterTab::AllZones, "#243042", "#ffffff", "#64748b");
}

void ZoneVisibilityDialog::updateKpiBadges() {
    if (!m_pvsGraph.contains(m_currentOriginZoneId)) {
        m_badgeStatusFrame->setStyleSheet(QStringLiteral("QFrame { background-color: #3b2a2a; border: 1px solid #7f1d1d; border-radius: 6px; }"));
        m_lblBadgeStatus->setStyleSheet(QStringLiteral("color: #fca5a5; font-weight: bold; font-size: 12px;"));
        m_lblBadgeStatus->setText(tr("⚠️ No PVS compiled data available. Rebuild level with F9."));
        m_lblBadgeTotal->setText(tr("Visible rooms: —"));
        m_lblBadgeDoors->setText(tr("Valid doorways: —"));
        return;
    }

    const auto& pvsInfo = m_pvsGraph[m_currentOriginZoneId];
    int totalVisible = static_cast<int>(pvsInfo.pathsToOtherZones.size());
    int totalZones = m_visZoneManager ? static_cast<int>(m_visZoneManager->zones().size()) : 0;
    int breachCount = 0;
    int doorCount = 0;

    for (auto it = pvsInfo.pathsToOtherZones.begin(); it != pvsInfo.pathsToOtherZones.end(); ++it) {
        bool hasBreach = false;
        for (const auto& conn : it.value()) {
            if (conn.isBreach || conn.isCrack) {
                hasBreach = true;
                break;
            }
        }
        if (hasBreach) breachCount++;
        else doorCount++;
    }

    if (breachCount > 0) {
        m_badgeStatusFrame->setStyleSheet(QStringLiteral("QFrame { background-color: #451a1a; border: 2px solid #ef4444; border-radius: 6px; }"));
        m_lblBadgeStatus->setStyleSheet(QStringLiteral("color: #fecaca; font-weight: bold; font-size: 12px;"));
        m_lblBadgeStatus->setText(tr("🚨 VISIBILITY LEAKS DETECTED: %1 room(s) rendered erroneously!").arg(breachCount));
        if (m_activeTab == FilterTab::DoorsOnly) {
            m_activeTab = FilterTab::BreachesOnly;
        }
    } else {
        m_badgeStatusFrame->setStyleSheet(QStringLiteral("QFrame { background-color: #13322b; border: 1px solid #10b981; border-radius: 6px; }"));
        m_lblBadgeStatus->setStyleSheet(QStringLiteral("color: #6ee7b7; font-weight: bold; font-size: 12px;"));
        m_lblBadgeStatus->setText(tr("✅ Clean Occlusion: All connections are valid doors and archways."));
        if (m_activeTab == FilterTab::BreachesOnly) {
            m_activeTab = FilterTab::DoorsOnly;
        }
    }

    m_lblBadgeTotal->setText(tr("Renders <b>%1</b> of <b>%2</b> total zones").arg(totalVisible).arg(totalZones));
    m_lblBadgeDoors->setText(tr("<b>%1</b> zones via clean doors").arg(doorCount));
}

int ZoneVisibilityDialog::getSelectedTargetZoneId() const {
    int row = m_tableZones->currentRow();
    if (row >= 0 && row < static_cast<int>(m_visibleTargetZoneIds.size())) {
        return m_visibleTargetZoneIds[row];
    }
    return -1;
}

const ZoneConnection* ZoneVisibilityDialog::findCulpritInChain(const std::vector<ZoneConnection>& chain) const {
    for (const auto& conn : chain) {
        if (conn.isBreach) return &conn;
    }
    for (const auto& conn : chain) {
        if (conn.isCrack) return &conn;
    }
    if (!chain.empty()) return &chain.front();
    return nullptr;
}

void ZoneVisibilityDialog::updateZoneList() {
    m_tableZones->setRowCount(0);
    m_visibleTargetZoneIds.clear();

    if (!m_pvsGraph.contains(m_currentOriginZoneId)) {
        updateChainFlow(-1);
        return;
    }

    const auto& pvsInfo = m_pvsGraph[m_currentOriginZoneId];
    QString filterText = m_searchFilter->text().trimmed();

    struct ZoneRow {
        int tid;
        QString name;
        QString cause;
        int hops;
        bool isBreach;
        bool isCrack;
    };

    std::vector<ZoneRow> rows;

    for (auto it = pvsInfo.pathsToOtherZones.begin(); it != pvsInfo.pathsToOtherZones.end(); ++it) {
        int tid = it.key();
        const auto& path = it.value();

        bool hasBreach = false;
        bool hasCrack = false;
        const ZoneConnection* firstCulprit = nullptr;

        for (const auto& conn : path) {
            if (conn.isBreach) {
                hasBreach = true;
                if (!firstCulprit) firstCulprit = &conn;
            } else if (conn.isCrack) {
                hasCrack = true;
                if (!firstCulprit) firstCulprit = &conn;
            }
        }

        if (m_activeTab == FilterTab::BreachesOnly && !hasBreach && !hasCrack) continue;
        if (m_activeTab == FilterTab::DoorsOnly && (hasBreach || hasCrack)) continue;

        QString roomName;
        if (m_visZoneManager) {
            const VisZone* tz = m_visZoneManager->getZone(tid);
            if (tz && !tz->name.isEmpty()) {
                roomName = QString("%1: %2 (Floor %3-%4)").arg(tid + 1).arg(tz->name).arg(tz->minFloor).arg(tz->maxFloor);
            } else if (tz) {
                roomName = QString("Zone %1 (Floor %2-%3)").arg(tid + 1).arg(tz->minFloor).arg(tz->maxFloor);
            }
        }
        if (roomName.isEmpty()) roomName = QString("Zone %1").arg(tid + 1);

        if (!filterText.isEmpty()) {
            if (!roomName.contains(filterText, Qt::CaseInsensitive) && !QString::number(tid + 1).contains(filterText)) {
                continue;
            }
        }

        QString causeStr;
        if (hasBreach && firstCulprit) {
            causeStr = firstCulprit->isHorizontal
                       ? tr("🚨 Floor Slab Breach at (%1, %2)").arg(firstCulprit->x1).arg(firstCulprit->y1)
                       : tr("🚨 Wall Breach at (%1, %2)").arg(firstCulprit->x1).arg(firstCulprit->y1);
        } else if (hasCrack && firstCulprit) {
            causeStr = tr("⚠️ Micro-seam at (%1, %2)").arg(firstCulprit->x1).arg(firstCulprit->y1);
        } else {
            causeStr = (path.size() == 1) ? tr("🚪 Direct Door") : tr("🚪 %1 Doors").arg(path.size());
        }

        ZoneRow r;
        r.tid = tid;
        r.name = roomName;
        r.cause = causeStr;
        r.hops = static_cast<int>(path.size());
        r.isBreach = hasBreach;
        r.isCrack = hasCrack;
        rows.push_back(r);
    }

    // Sort: Breaches first, then by hop count
    std::sort(rows.begin(), rows.end(), [](const ZoneRow& a, const ZoneRow& b) {
        if (a.isBreach != b.isBreach) return a.isBreach > b.isBreach;
        if (a.isCrack != b.isCrack) return a.isCrack > b.isCrack;
        if (a.hops != b.hops) return a.hops < b.hops;
        return a.tid < b.tid;
    });

    m_tableZones->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[i];
        m_visibleTargetZoneIds.push_back(r.tid);

        QTableWidgetItem* itemRoom = new QTableWidgetItem(r.name);
        if (r.isBreach) {
            itemRoom->setIcon(QIcon(":/icons/leak_error.png"));
            itemRoom->setForeground(QColor("#fca5a5"));
        } else if (r.isCrack) {
            itemRoom->setIcon(QIcon(":/icons/leak_warn.png"));
            itemRoom->setForeground(QColor("#fde047"));
        } else {
            itemRoom->setForeground(QColor("#f1f5f9"));
        }

        QTableWidgetItem* itemCause = new QTableWidgetItem(r.cause);
        if (r.isBreach) itemCause->setForeground(QColor("#ef4444"));
        else if (r.isCrack) itemCause->setForeground(QColor("#f59e0b"));
        else itemCause->setForeground(QColor("#38bdf8"));

        QTableWidgetItem* itemHops = new QTableWidgetItem(QString::number(r.hops));
        itemHops->setTextAlignment(Qt::AlignCenter);

        m_tableZones->setItem(i, 0, itemRoom);
        m_tableZones->setItem(i, 1, itemCause);
        m_tableZones->setItem(i, 2, itemHops);
    }

    if (!rows.empty()) {
        m_tableZones->selectRow(0);
        updateChainFlow(rows[0].tid);
    } else {
        updateChainFlow(-1);
    }
}

void ZoneVisibilityDialog::onZoneSelectionChanged() {
    int tid = getSelectedTargetZoneId();
    updateChainFlow(tid);
}

void ZoneVisibilityDialog::onZoneTableDoubleClicked(int row, int col) {
    Q_UNUSED(row);
    Q_UNUSED(col);
    onJumpToCulpritClicked();
}

void ZoneVisibilityDialog::updateChainFlow(int targetZoneId) {
    // Clear existing cards
    QLayoutItem* item = nullptr;
    while ((item = m_chainLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    m_btnJumpToCulprit->setEnabled(false);
    m_btnEditCulprit->setEnabled(false);

    if (targetZoneId < 0 || !m_pvsGraph.contains(m_currentOriginZoneId)) {
        m_lblChainTitle->setText(tr("No room selected"));
        m_lblChainSubtitle->setText(QString());
        m_frameDiagnosis->setStyleSheet(QStringLiteral("QFrame { background-color: #1e293b; border: 1px solid #334155; border-radius: 6px; }"));
        m_lblDiagnosisIcon->setText(QStringLiteral("ℹ️"));
        m_lblDiagnosisText->setText(tr("Select a room from the left list to see the full causal visibility traversal path."));
        emit tracePathSelected({});
        return;
    }

    const auto& pvsInfo = m_pvsGraph[m_currentOriginZoneId];
    if (!pvsInfo.pathsToOtherZones.contains(targetZoneId)) {
        emit tracePathSelected({});
        return;
    }

    const auto& path = pvsInfo.pathsToOtherZones[targetZoneId];
    emit tracePathSelected(path);

    bool hasBreach = false;
    bool hasCrack = false;
    const ZoneConnection* culprit = nullptr;

    for (const auto& conn : path) {
        if (conn.isBreach) {
            hasBreach = true;
            if (!culprit) culprit = &conn;
        } else if (conn.isCrack) {
            hasCrack = true;
            if (!culprit) culprit = &conn;
        }
    }

    QString targetZoneName = QString("Zone %1").arg(targetZoneId + 1);
    if (m_visZoneManager) {
        const VisZone* tz = m_visZoneManager->getZone(targetZoneId);
        if (tz && !tz->name.isEmpty()) targetZoneName = QString("Zone %1: %2").arg(tz->id + 1).arg(tz->name);
    }

    QString originZoneName = QString("Zone %1").arg(m_currentOriginZoneId + 1);
    if (m_visZoneManager) {
        const VisZone* oz = m_visZoneManager->getZone(m_currentOriginZoneId);
        if (oz && !oz->name.isEmpty()) originZoneName = QString("Zone %1: %2").arg(oz->id + 1).arg(oz->name);
    }

    m_lblChainTitle->setText(tr("🎯 Why is %1 rendered from %2?").arg(targetZoneName).arg(originZoneName));
    m_lblChainSubtitle->setText(tr("Shortest traversal path: %1 %2").arg(path.size()).arg(path.size() == 1 ? tr("hop") : tr("hops")));

    if (hasBreach && culprit) {
        int culpritStepIdx = 2;
        for (size_t i = 0; i < path.size(); ++i) {
            if (&path[i] == culprit) {
                culpritStepIdx = static_cast<int>(i) + 2;
                break;
            }
        }
        m_frameDiagnosis->setStyleSheet(QStringLiteral("QFrame { background-color: #451a1a; border: 2px solid #ef4444; border-radius: 8px; }"));
        m_lblDiagnosisIcon->setText(QStringLiteral("🚨"));
        QString defectType = culprit->isHorizontal ? tr("Inter-floor Slab Breach") : tr("Solid Wall Breach");
        m_lblDiagnosisText->setText(tr(
            "<b>VISIBILITY LEAK DETECTED:</b> This room is erroneously rendered due to an illegal <b>%1</b>!<br>"
            "The camera line-of-sight penetrates through solid geometry on Step %2 at Floor %3, cell (%4, %5).<br>"
            "<b>Sealing this defect will immediately restore occlusion culling and hide this room.</b>"
        ).arg(defectType).arg(culpritStepIdx).arg(culprit->layer).arg(culprit->x1).arg(culprit->y1));

        m_btnJumpToCulprit->setEnabled(true);
        m_btnEditCulprit->setEnabled(true);
    } else if (hasCrack && culprit) {
        m_frameDiagnosis->setStyleSheet(QStringLiteral("QFrame { background-color: #3f2d1a; border: 1px solid #f59e0b; border-radius: 8px; }"));
        m_lblDiagnosisIcon->setText(QStringLiteral("⚠️"));
        m_lblDiagnosisText->setText(tr(
            "<b>Micro-Seam in Mesh:</b> This room is reached through a small CSG seam/crack at Floor %1 (%2, %3)."
        ).arg(culprit->layer).arg(culprit->x1).arg(culprit->y1));

        m_btnJumpToCulprit->setEnabled(true);
        m_btnEditCulprit->setEnabled(false);
    } else {
        m_frameDiagnosis->setStyleSheet(QStringLiteral("QFrame { background-color: #13322b; border: 1px solid #10b981; border-radius: 8px; }"));
        m_lblDiagnosisIcon->setText(QStringLiteral("✅"));
        m_lblDiagnosisText->setText(tr(
            "<b>Standard Visibility:</b> This room is visible through a normal chain of open doorways and windows.<br>"
            "No geometry defects or wall leaks were detected in this path."
        ));

        m_btnJumpToCulprit->setEnabled(false);
        m_btnEditCulprit->setEnabled(false);
    }

    // Helper lambda to create a clean step card
    auto createCard = [&](const QString& badge, const QString& title, const QString& desc, const QString& bg, const QString& border,
                          int layer = -1, int cx = -1, int cy = -1, bool hasEdit = false) {
        QFrame* card = new QFrame(m_chainWidget);
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(QString("QFrame { background-color: %1; border: %2; border-radius: 6px; padding: 6px 10px; }").arg(bg, border));

        QVBoxLayout* cLayout = new QVBoxLayout(card);
        cLayout->setContentsMargins(8, 6, 8, 6);
        cLayout->setSpacing(4);

        QHBoxLayout* hRow = new QHBoxLayout();
        QLabel* lblBadge = new QLabel(badge, card);
        lblBadge->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px;"));
        hRow->addWidget(lblBadge);

        QLabel* lblTitle = new QLabel(title, card);
        lblTitle->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #f1f5f9;"));
        hRow->addWidget(lblTitle, 1);

        if (layer >= 0 && cx >= 0 && cy >= 0) {
            QPushButton* btnJump = new QPushButton(tr("🔍 Show on Map"), card);
            btnJump->setStyleSheet(QStringLiteral("QPushButton { font-size: 11px; padding: 3px 8px; background-color: #2563eb; color: #ffffff; border: none; border-radius: 4px; }"
                                                 "QPushButton:hover { background-color: #3b82f6; }"));
            connect(btnJump, &QPushButton::clicked, this, [this, layer, cx, cy]() {
                emit cellSelected(layer, cx, cy);
            });
            hRow->addWidget(btnJump);

            if (hasEdit) {
                QPushButton* btnEdit = new QPushButton(tr("🧱 Edit Segment"), card);
                btnEdit->setStyleSheet(QStringLiteral("QPushButton { font-size: 11px; padding: 3px 8px; background-color: #b91c1c; color: #ffffff; border: none; border-radius: 4px; font-weight: bold; }"
                                                     "QPushButton:hover { background-color: #dc2626; }"));
                connect(btnEdit, &QPushButton::clicked, this, [this, layer, cx, cy]() {
                    emit editSegmentRequested(layer, cx, cy);
                });
                hRow->addWidget(btnEdit);
            }
        }

        cLayout->addLayout(hRow);

        if (!desc.isEmpty()) {
            QLabel* lblDesc = new QLabel(desc, card);
            lblDesc->setWordWrap(true);
            lblDesc->setStyleSheet(QStringLiteral("font-size: 11px; color: #cbd5e1;"));
            cLayout->addWidget(lblDesc);
        }

        m_chainLayout->addWidget(card);
    };

    auto createArrow = [&]() {
        QLabel* arrow = new QLabel(QStringLiteral("│\n▼"), m_chainWidget);
        arrow->setAlignment(Qt::AlignCenter);
        arrow->setStyleSheet(QStringLiteral("color: #64748b; font-weight: bold; font-size: 12px; line-height: 10px;"));
        m_chainLayout->addWidget(arrow);
    };

    // 1. Origin Room Card
    int originFloor = 0;
    if (m_visZoneManager) {
        const VisZone* oz = m_visZoneManager->getZone(m_currentOriginZoneId);
        if (oz) originFloor = oz->minFloor;
    }
    createCard(QStringLiteral("📍"), tr("Step 1: Origin Room — %1 (Floor %2)").arg(originZoneName).arg(originFloor),
               tr("Player camera position. Visibility ray starts from here."),
               "#1e293b", "1px solid #3b82f6");

    // 2. Traversal Steps
    for (size_t i = 0; i < path.size(); ++i) {
        createArrow();
        const auto& conn = path[i];

        if (conn.isBreach) {
            QString bTitle = conn.isHorizontal
                ? tr("🚨 STEP %1: INTER-FLOOR SLAB BREACH at Floor %2 (%3, %4)")
                    .arg(i + 2).arg(conn.layer).arg(conn.x1).arg(conn.y1)
                : tr("🚨 STEP %1: INTERNAL WALL BREACH at Floor %2 (%3, %4) ↔ (%5, %6)")
                    .arg(i + 2).arg(conn.layer).arg(conn.x1).arg(conn.y1).arg(conn.x2).arg(conn.y2);

            QString bDesc = tr(
                "⚠️ <b>ROOT CAUSE OF LEAK:</b> A solid %1 is penetrated by a 100×100 BSP portal with no door or window.<br>%2"
            ).arg(conn.isHorizontal ? tr("floor/ceiling slab") : tr("wall")).arg(conn.description);

            createCard(QStringLiteral("🚨"), bTitle, bDesc, "#451a1a", "2px solid #ef4444", conn.layer, conn.x1, conn.y1, true);
        } else if (conn.isCrack) {
            createCard(QStringLiteral("⚠️"), tr("Step %1: Micro-Seam in Mesh at Floor %2 (%3, %4)").arg(i + 2).arg(conn.layer).arg(conn.x1).arg(conn.y1),
                       conn.description, "#3f2d1a", "1px solid #f59e0b", conn.layer, conn.x1, conn.y1, false);
        } else if (conn.isDoorWin) {
            createCard(QStringLiteral("🚪"), tr("Step %1: Door / Window Passage at Floor %2 (%3, %4)").arg(i + 2).arg(conn.layer).arg(conn.x1).arg(conn.y1),
                       tr("Valid line of sight between adjacent rooms via door or window portal."),
                       "#182230", "1px solid #334155", conn.layer, conn.x1, conn.y1, false);
        } else {
            createCard(QStringLiteral("🌌"), tr("Step %1: Open Transition at Floor %2 (%3, %4)").arg(i + 2).arg(conn.layer).arg(conn.x1).arg(conn.y1),
                       tr("Open hallway or wall opening without doors."),
                       "#142520", "1px solid #1e3a30", conn.layer, conn.x1, conn.y1, false);
        }

        // Room reached after this step
        if (i == path.size() - 1) {
            createArrow();
            int destFloor = 0;
            if (m_visZoneManager) {
                const VisZone* dz = m_visZoneManager->getZone(targetZoneId);
                if (dz) destFloor = dz->minFloor;
            }
            if (hasBreach) {
                createCard(QStringLiteral("🎯"), tr("Result: Erroneously Rendered Room — %1 (Floor %2)").arg(targetZoneName).arg(destFloor),
                           tr("This entire zone is drawn by the game engine due to the leak above! Fixing the breach will hide it."),
                           "#3b1818", "2px solid #dc2626");
            } else {
                createCard(QStringLiteral("🎯"), tr("Result: Visibly Rendered Room — %1 (Floor %2)").arg(targetZoneName).arg(destFloor),
                           tr("Naturally visible through open doorways."),
                           "#182820", "1px solid #10b981");
            }
        }
    }

    m_chainLayout->addStretch();
}

void ZoneVisibilityDialog::onJumpToCulpritClicked() {
    int tid = getSelectedTargetZoneId();
    if (tid < 0 || !m_pvsGraph.contains(m_currentOriginZoneId)) return;

    const auto& path = m_pvsGraph[m_currentOriginZoneId].pathsToOtherZones[tid];
    const ZoneConnection* culprit = findCulpritInChain(path);
    if (culprit) {
        emit cellSelected(culprit->layer, culprit->x1, culprit->y1);
    }
}

void ZoneVisibilityDialog::onEditWallAtCulpritClicked() {
    int tid = getSelectedTargetZoneId();
    if (tid < 0 || !m_pvsGraph.contains(m_currentOriginZoneId)) return;

    const auto& path = m_pvsGraph[m_currentOriginZoneId].pathsToOtherZones[tid];
    const ZoneConnection* culprit = findCulpritInChain(path);
    if (culprit) {
        emit editSegmentRequested(culprit->layer, culprit->x1, culprit->y1);
    }
}
