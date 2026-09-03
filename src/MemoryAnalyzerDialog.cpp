#include "MemoryAnalyzerDialog.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>

MemoryAnalyzerDialog::MemoryAnalyzerDialog(std::shared_ptr<FPSCMap> map, QWidget* parent)
    : QDialog(parent)
    , m_map(map)
{
    QString mapName = m_map ? m_map->mapName : QStringLiteral("No Map");
    setWindowTitle(QString("FPS Creator Memory Footprint Analyzer — %1").arg(mapName));
    resize(1060, 720);
    setMinimumSize(850, 520);

    m_report = MemoryAnalyzer::analyze(m_map);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // 1. KPI Summary Cards
    QHBoxLayout* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(8);

    cardsLayout->addWidget(createCard(QStringLiteral("TOTAL LEVEL RAM"), m_cardTotalVal, "#3498db"));
    cardsLayout->addWidget(createCard(QStringLiteral("SEGMENTS (ROOMS)"), m_cardSegVal, "#e67e22"));
    cardsLayout->addWidget(createCard(QStringLiteral("ENTITIES (PROPS)"), m_cardEntVal, "#9b59b6"));
    cardsLayout->addWidget(createCard(QStringLiteral("UNIVERSE & LIGHTMAPS"), m_cardUniVal, "#1abc9c"));
    cardsLayout->addWidget(createCard(QStringLiteral("ENGINE & D3D BASE"), m_cardEngineVal, "#f1c40f"));
    mainLayout->addLayout(cardsLayout);

    // 2. Engine Memory Limit Progress Bar
    QGroupBox* gaugeGroup = new QGroupBox(QStringLiteral("DirectX 9 / 32-bit Process Memory Budget (Limit: ~1850 MB)"), this);
    QVBoxLayout* gaugeLayout = new QVBoxLayout(gaugeGroup);
    gaugeLayout->setContentsMargins(10, 8, 10, 8);
    gaugeLayout->setSpacing(4);

    m_limitLabel = new QLabel(gaugeGroup);
    m_limitLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    gaugeLayout->addWidget(m_limitLabel);

    m_limitProgress = new QProgressBar(gaugeGroup);
    m_limitProgress->setRange(0, 1850);
    m_limitProgress->setTextVisible(true);
    m_limitProgress->setFixedHeight(22);
    gaugeLayout->addWidget(m_limitProgress);

    mainLayout->addWidget(gaugeGroup);

    // 3. Search & Filter
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Filter table by name, category, or texture path..."));
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(new QLabel(QStringLiteral("Search:"), this));
    searchLayout->addWidget(m_searchEdit, 1);
    mainLayout->addLayout(searchLayout);

    // 4. Tab Widget
    m_tabWidget = new QTabWidget(this);

    // Tab 1: Entities Table
    m_entityTable = new QTableWidget(this);
    m_entityTable->setColumnCount(10);
    m_entityTable->setHorizontalHeaderLabels({
        QStringLiteral("Icon"),
        QStringLiteral("Entity Profile"),
        QStringLiteral("Category"),
        QStringLiteral("Placed"),
        QStringLiteral("Model (.X)"),
        QStringLiteral("Texture RAM"),
        QStringLiteral("Audio"),
        QStringLiteral("RAM / Inst"),
        QStringLiteral("Total RAM"),
        QStringLiteral("Alerts / Advice")
    });
    m_entityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_entityTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    m_entityTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
    m_entityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_entityTable->setAlternatingRowColors(true);
    m_entityTable->verticalHeader()->setVisible(false);
    m_entityTable->setIconSize(QSize(28, 28));
    m_tabWidget->addTab(m_entityTable, QStringLiteral("Entities"));

    // Tab 2: Segments Table
    m_segmentTable = new QTableWidget(this);
    m_segmentTable->setColumnCount(9);
    m_segmentTable->setHorizontalHeaderLabels({
        QStringLiteral("Icon"),
        QStringLiteral("Segment Name"),
        QStringLiteral("Parts"),
        QStringLiteral("Placed Blocks"),
        QStringLiteral("Mesh RAM"),
        QStringLiteral("Diffuse RAM"),
        QStringLiteral("Normal/Spec RAM"),
        QStringLiteral("Total RAM"),
        QStringLiteral("Alerts")
    });
    m_segmentTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_segmentTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    m_segmentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_segmentTable->setAlternatingRowColors(true);
    m_segmentTable->verticalHeader()->setVisible(false);
    m_segmentTable->setIconSize(QSize(28, 28));
    m_tabWidget->addTab(m_segmentTable, QStringLiteral("Segments (Architecture)"));

    // Tab 3: Universe & Engine Breakdown Table
    m_engineTable = new QTableWidget(this);
    m_engineTable->setColumnCount(4);
    m_engineTable->setHorizontalHeaderLabels({
        QStringLiteral("Component"),
        QStringLiteral("Type / Format"),
        QStringLiteral("Estimated RAM"),
        QStringLiteral("Description")
    });
    m_engineTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_engineTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_engineTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_engineTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_engineTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_engineTable->setAlternatingRowColors(true);
    m_engineTable->verticalHeader()->setVisible(false);
    m_tabWidget->addTab(m_engineTable, QStringLiteral("Universe & Engine Breakdown"));

    // Tab 4: Optimization Recommendations
    QWidget* tipsTab = new QWidget(this);
    QVBoxLayout* tipsLayout = new QVBoxLayout(tipsTab);
    m_tipsEdit = new QTextEdit(tipsTab);
    m_tipsEdit->setReadOnly(true);
    m_tipsEdit->setStyleSheet("background: #1e222d; color: #f1c40f; border: 1px solid #3d4455; font-size: 12px; font-family: monospace;");
    tipsLayout->addWidget(m_tipsEdit);
    m_tabWidget->addTab(tipsTab, QStringLiteral("Optimization Tips"));

    mainLayout->addWidget(m_tabWidget, 1);

    // 5. Action Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_copyBtn = new QPushButton(QStringLiteral("Copy Full Report to Clipboard"), this);
    m_exportBtn = new QPushButton(QStringLiteral("Export CSV Report..."), this);
    QPushButton* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setDefault(true);

    btnLayout->addWidget(m_copyBtn);
    btnLayout->addWidget(m_exportBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &MemoryAnalyzerDialog::onSearchChanged);
    connect(m_copyBtn, &QPushButton::clicked, this, &MemoryAnalyzerDialog::onCopyReport);
    connect(m_exportBtn, &QPushButton::clicked, this, &MemoryAnalyzerDialog::onExportCSV);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    populateUI();
}

void MemoryAnalyzerDialog::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    QString mapName = m_map ? m_map->mapName : QStringLiteral("No Map");
    setWindowTitle(QString("FPS Creator Memory Footprint Analyzer — %1").arg(mapName));
    m_report = MemoryAnalyzer::analyze(m_map);
    populateUI();
}

QWidget* MemoryAnalyzerDialog::createCard(const QString& title, QLabel*& outValueLabel, const QString& color) {
    QWidget* card = new QWidget(this);
    card->setStyleSheet("background: #1f2430; border: 1px solid #353b4b; border-radius: 6px; padding: 6px;");
    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(2);

    outValueLabel = new QLabel("0 MB", card);
    outValueLabel->setStyleSheet(QString("font-size: 15px; font-weight: bold; color: %1;").arg(color));
    outValueLabel->setAlignment(Qt::AlignCenter);
    lay->addWidget(outValueLabel);

    QLabel* titLbl = new QLabel(title, card);
    titLbl->setStyleSheet("font-size: 10px; color: #8892b0; font-weight: bold;");
    titLbl->setAlignment(Qt::AlignCenter);
    lay->addWidget(titLbl);

    return card;
}

void MemoryAnalyzerDialog::populateUI() {
    float totalMb = m_report.totalEstimatedRamBytes / (1024.0f * 1024.0f);
    float segMb = m_report.totalSegmentRamBytes / (1024.0f * 1024.0f);
    float entMb = m_report.totalEntityRamBytes / (1024.0f * 1024.0f);
    float uniMb = (m_report.universeCsgRamBytes + m_report.lightmapsRamBytes) / (1024.0f * 1024.0f);
    float engineMb = m_report.engineBaselineRamBytes / (1024.0f * 1024.0f);

    if (m_cardTotalVal) m_cardTotalVal->setText(QString("%1 MB").arg(totalMb, 0, 'f', 1));
    if (m_cardSegVal) m_cardSegVal->setText(QString("%1 MB").arg(segMb, 0, 'f', 1));
    if (m_cardEntVal) m_cardEntVal->setText(QString("%1 MB").arg(entMb, 0, 'f', 1));
    if (m_cardUniVal) m_cardUniVal->setText(QString("%1 MB").arg(uniMb, 0, 'f', 1));
    if (m_cardEngineVal) m_cardEngineVal->setText(QString("%1 MB").arg(engineMb, 0, 'f', 1));

    if (m_limitLabel) {
        m_limitLabel->setText(QString("Level RAM Usage: %1 MB / 1850 MB (%2%) — Status: %3")
            .arg(totalMb, 0, 'f', 1)
            .arg(m_report.engineLimitPercent, 0, 'f', 1)
            .arg(m_report.riskLevel));
    }

    if (m_limitProgress) {
        m_limitProgress->setValue(static_cast<int>(totalMb));
        m_limitProgress->setFormat(QString("%1 MB (%p%)").arg(totalMb, 0, 'f', 1));
        QString progressColor = "#2ecc71";
        if (m_report.engineLimitPercent > 75.0f) progressColor = "#e74c3c";
        else if (m_report.engineLimitPercent > 50.0f) progressColor = "#f39c12";

        m_limitProgress->setStyleSheet(QString(
            "QProgressBar { border: 1px solid #3d4455; border-radius: 4px; text-align: center; background: #1a1e28; color: white; }"
            "QProgressBar::chunk { background-color: %1; border-radius: 3px; }"
        ).arg(progressColor));
    }

    // Update tab titles with counts
    m_tabWidget->setTabText(0, QString("Entities (%1 types, %2 placed)").arg(m_report.uniqueEntityTypesCount).arg(m_report.totalPlacedEntities));
    m_tabWidget->setTabText(1, QString("Segments (%1 types, %2 blocks)").arg(m_report.uniqueSegmentTypesCount).arg(m_report.totalPlacedSegmentBlocks));

    QString filter = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();

    // 1. Populate Entities Table
    m_entityTable->setSortingEnabled(false);
    m_entityTable->setRowCount(0);

    for (int i = 0; i < m_report.entityItems.size(); ++i) {
        const auto& itm = m_report.entityItems[i];

        if (!filter.isEmpty()) {
            bool match = itm.name.toLower().contains(filter) ||
                         itm.relPath.toLower().contains(filter) ||
                         entityCategoryToString(itm.category).toLower().contains(filter);
            if (!match) continue;
        }

        int row = m_entityTable->rowCount();
        m_entityTable->insertRow(row);

        // Col 0: Icon
        QPixmap iconPx;
        if (!itm.iconBmpPath.isEmpty()) iconPx = AssetManager::instance().loadIcon(itm.iconBmpPath);
        QTableWidgetItem* iconItm = new QTableWidgetItem();
        if (!iconPx.isNull()) iconItm->setIcon(QIcon(iconPx));
        m_entityTable->setItem(row, 0, iconItm);

        // Col 1: Name
        QTableWidgetItem* nameItm = new QTableWidgetItem(itm.name);
        nameItm->setToolTip(itm.relPath);
        m_entityTable->setItem(row, 1, nameItm);

        // Col 2: Category
        QTableWidgetItem* catItm = new QTableWidgetItem(entityCategoryToString(itm.category));
        m_entityTable->setItem(row, 2, catItm);

        // Col 3: Instances
        QTableWidgetItem* cntItm = new QTableWidgetItem();
        cntItm->setData(Qt::DisplayRole, itm.instanceCount);
        cntItm->setTextAlignment(Qt::AlignCenter);
        m_entityTable->setItem(row, 3, cntItm);

        // Col 4: Mesh Size
        QTableWidgetItem* meshItm = new QTableWidgetItem();
        meshItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.meshSizeBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        meshItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_entityTable->setItem(row, 4, meshItm);

        // Col 5: Texture RAM Size
        qint64 texTotal = itm.textureRamBytes + itm.normalRamBytes + itm.specularRamBytes;
        QTableWidgetItem* texItm = new QTableWidgetItem();
        texItm->setData(Qt::DisplayRole, QString("%1 MB").arg(texTotal / (1024.0f * 1024.0f), 0, 'f', 2));
        texItm->setToolTip(QString("Diffuse: %1 MB\nNormals: %2 MB\nSpecular: %3 MB\nRes: %4x%5")
            .arg(itm.textureRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.normalRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.specularRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.texWidth).arg(itm.texHeight));
        texItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_entityTable->setItem(row, 5, texItm);

        // Col 6: Audio Size
        QTableWidgetItem* audItm = new QTableWidgetItem();
        audItm->setData(Qt::DisplayRole, QString("%1 KB").arg(itm.audioSizeBytes / 1024.0f, 0, 'f', 1));
        audItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_entityTable->setItem(row, 6, audItm);

        // Col 7: RAM / Inst
        QTableWidgetItem* rpiItm = new QTableWidgetItem();
        rpiItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.ramPerInstanceBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        rpiItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_entityTable->setItem(row, 7, rpiItm);

        // Col 8: Total RAM
        QTableWidgetItem* totItm = new QTableWidgetItem();
        totItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.totalTypeRamBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        totItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (itm.totalTypeRamBytes >= 15LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(235, 77, 75));
        } else if (itm.totalTypeRamBytes >= 5LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(241, 196, 15));
        }
        m_entityTable->setItem(row, 8, totItm);

        // Col 9: Alerts
        QTableWidgetItem* alrItm = new QTableWidgetItem(itm.warnings.join(", "));
        if (!itm.warnings.isEmpty()) {
            alrItm->setForeground(QColor(243, 156, 18));
        }
        m_entityTable->setItem(row, 9, alrItm);
    }
    m_entityTable->setSortingEnabled(true);

    // 2. Populate Segments Table
    m_segmentTable->setSortingEnabled(false);
    m_segmentTable->setRowCount(0);

    for (int i = 0; i < m_report.segmentItems.size(); ++i) {
        const auto& itm = m_report.segmentItems[i];

        if (!filter.isEmpty()) {
            bool match = itm.name.toLower().contains(filter) ||
                         itm.relPath.toLower().contains(filter);
            if (!match) continue;
        }

        int row = m_segmentTable->rowCount();
        m_segmentTable->insertRow(row);

        // Col 0: Icon
        QPixmap iconPx;
        if (!itm.iconBmpPath.isEmpty()) iconPx = AssetManager::instance().loadIcon(itm.iconBmpPath);
        QTableWidgetItem* iconItm = new QTableWidgetItem();
        if (!iconPx.isNull()) iconItm->setIcon(QIcon(iconPx));
        m_segmentTable->setItem(row, 0, iconItm);

        // Col 1: Name
        QTableWidgetItem* nameItm = new QTableWidgetItem(itm.name);
        nameItm->setToolTip(itm.relPath);
        m_segmentTable->setItem(row, 1, nameItm);

        // Col 2: Parts
        QTableWidgetItem* partsItm = new QTableWidgetItem();
        partsItm->setData(Qt::DisplayRole, itm.partCount);
        partsItm->setTextAlignment(Qt::AlignCenter);
        m_segmentTable->setItem(row, 2, partsItm);

        // Col 3: Placed Blocks
        QTableWidgetItem* placedItm = new QTableWidgetItem();
        placedItm->setData(Qt::DisplayRole, itm.placedCount);
        placedItm->setTextAlignment(Qt::AlignCenter);
        m_segmentTable->setItem(row, 3, placedItm);

        // Col 4: Mesh RAM
        QTableWidgetItem* meshItm = new QTableWidgetItem();
        meshItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.meshSizeBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        meshItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_segmentTable->setItem(row, 4, meshItm);

        // Col 5: Diffuse RAM
        QTableWidgetItem* diffItm = new QTableWidgetItem();
        diffItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.diffuseRamBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        diffItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_segmentTable->setItem(row, 5, diffItm);

        // Col 6: Normal/Spec RAM
        qint64 normSpec = itm.normalRamBytes + itm.specularRamBytes;
        QTableWidgetItem* nsItm = new QTableWidgetItem();
        nsItm->setData(Qt::DisplayRole, QString("%1 MB").arg(normSpec / (1024.0f * 1024.0f), 0, 'f', 2));
        nsItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_segmentTable->setItem(row, 6, nsItm);

        // Col 7: Total RAM
        QTableWidgetItem* totItm = new QTableWidgetItem();
        totItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.totalTypeRamBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        totItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (itm.totalTypeRamBytes >= 15LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(235, 77, 75));
        } else if (itm.totalTypeRamBytes >= 5LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(241, 196, 15));
        }
        m_segmentTable->setItem(row, 7, totItm);

        // Col 8: Alerts
        QTableWidgetItem* alrItm = new QTableWidgetItem(itm.warnings.join(", "));
        if (!itm.warnings.isEmpty()) {
            alrItm->setForeground(QColor(243, 156, 18));
        }
        m_segmentTable->setItem(row, 8, alrItm);
    }
    m_segmentTable->setSortingEnabled(true);

    // 3. Populate Universe & Engine Breakdown Table
    m_engineTable->setRowCount(0);
    auto addEngineRow = [&](const QString& comp, const QString& type, float mb, const QString& desc) {
        int r = m_engineTable->rowCount();
        m_engineTable->insertRow(r);
        m_engineTable->setItem(r, 0, new QTableWidgetItem(comp));
        m_engineTable->setItem(r, 1, new QTableWidgetItem(type));
        QTableWidgetItem* ramItm = new QTableWidgetItem(QString("%1 MB").arg(mb, 0, 'f', 1));
        ramItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_engineTable->setItem(r, 2, ramItm);
        m_engineTable->setItem(r, 3, new QTableWidgetItem(desc));
    };

    addEngineRow(QStringLiteral("Universe CSG Geometry"), QStringLiteral("universe.dbo"),
                 m_report.universeCsgRamBytes / (1024.0f * 1024.0f),
                 QStringLiteral("Compiled CSG BSP tree, portal connectivity graphs, and static world trimeshes"));

    addEngineRow(QStringLiteral("Level Lightmaps"), QStringLiteral("D3D Surfaces"),
                 m_report.lightmapsRamBytes / (1024.0f * 1024.0f),
                 QStringLiteral("Pre-calculated static radiance lightmaps baked across all sector surfaces"));

    addEngineRow(QStringLiteral("D3D9 Render Targets & Buffers"), QStringLiteral("D3DSURFACE9"),
                 40.0f,
                 QStringLiteral("1080p Backbuffer, 32-bit Depth/Stencil Buffer, Dynamic Shadow Map, Post-FX Bloom Buffers"));

    addEngineRow(QStringLiteral("Engine & Physics DLLs"), QStringLiteral("Runtime Code"),
                 25.0f,
                 QStringLiteral("FPSC-Game.exe core, DarkBasic Pro runtime modules, ODE physics engine, DirectSound mixer"));

    // 4. Update Optimization Tips
    if (m_tipsEdit) {
        if (!m_report.optimizationTips.isEmpty()) {
            m_tipsEdit->setText("• " + m_report.optimizationTips.join("\n• "));
        } else {
            m_tipsEdit->setText("Level memory footprint is well within limits. No immediate optimizations required.");
        }
    }
}

void MemoryAnalyzerDialog::onSearchChanged(const QString&) {
    populateUI();
}

void MemoryAnalyzerDialog::onCopyReport() {
    QString txt;
    QTextStream ss(&txt);
    ss << "=== FPS CREATOR LEVEL MEMORY FOOTPRINT REPORT ===\n";
    ss << "Map: " << (m_map ? m_map->mapName : "Unknown") << "\n";
    ss << "Total Estimated Level RAM: " << (m_report.totalEstimatedRamBytes / (1024.0 * 1024.0)) << " MB\n";
    ss << "  - Segment Architecture: " << (m_report.totalSegmentRamBytes / (1024.0 * 1024.0)) << " MB ("
       << m_report.uniqueSegmentTypesCount << " unique types, " << m_report.totalPlacedSegmentBlocks << " blocks)\n";
    ss << "  - Placed Entities: " << (m_report.totalEntityRamBytes / (1024.0 * 1024.0)) << " MB ("
       << m_report.uniqueEntityTypesCount << " unique types, " << m_report.totalPlacedEntities << " placed)\n";
    ss << "  - Universe CSG & Lightmaps: " << ((m_report.universeCsgRamBytes + m_report.lightmapsRamBytes) / (1024.0 * 1024.0)) << " MB\n";
    ss << "  - Engine Baseline & D3D Buffers: " << (m_report.engineBaselineRamBytes / (1024.0 * 1024.0)) << " MB\n";
    ss << "Engine 32-bit Limit Usage: " << m_report.engineLimitPercent << "% (Status: " << m_report.riskLevel << ")\n\n";

    ss << "Top Heavy Segments:\n";
    for (int i = 0; i < qMin(5, m_report.segmentItems.size()); ++i) {
        const auto& itm = m_report.segmentItems[i];
        ss << QString("  %1. %2 (%3 placed) -> %4 MB RAM [Diffuse: %5 MB, Normal/Spec: %6 MB]\n")
            .arg(i + 1)
            .arg(itm.name)
            .arg(itm.placedCount)
            .arg(itm.totalTypeRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.diffuseRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg((itm.normalRamBytes + itm.specularRamBytes) / (1024.0 * 1024.0), 0, 'f', 2);
    }

    ss << "\nTop Heavy Entities:\n";
    for (int i = 0; i < qMin(5, m_report.entityItems.size()); ++i) {
        const auto& itm = m_report.entityItems[i];
        ss << QString("  %1. %2 (%3 instances) -> %4 MB RAM [Texture: %5 MB, Model: %6 KB]\n")
            .arg(i + 1)
            .arg(itm.name)
            .arg(itm.instanceCount)
            .arg(itm.totalTypeRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg((itm.textureRamBytes + itm.normalRamBytes + itm.specularRamBytes) / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.meshSizeBytes / 1024.0, 0, 'f', 1);
    }

    if (!m_report.optimizationTips.isEmpty()) {
        ss << "\nOptimization Tips:\n";
        for (const QString& tip : m_report.optimizationTips) {
            ss << "  * " << tip << "\n";
        }
    }

    QApplication::clipboard()->setText(txt);
    QMessageBox::information(this, QStringLiteral("Report Copied"), QStringLiteral("Full memory report copied to clipboard."));
}

void MemoryAnalyzerDialog::onExportCSV() {
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("Export Memory Report CSV"), QString(), QStringLiteral("CSV Files (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Failed to open file for writing."));
        return;
    }

    QTextStream ss(&f);
    ss << "Type,Name,Path,Count,MeshRamBytes,DiffuseRamBytes,NormalSpecRamBytes,TotalRamBytes,Alerts\n";

    for (const auto& s : m_report.segmentItems) {
        ss << QString("\"Segment\",\"%1\",\"%2\",%3,%4,%5,%6,%7,\"%8\"\n")
            .arg(s.name)
            .arg(s.relPath)
            .arg(s.placedCount)
            .arg(s.meshSizeBytes)
            .arg(s.diffuseRamBytes)
            .arg(s.normalRamBytes + s.specularRamBytes)
            .arg(s.totalTypeRamBytes)
            .arg(s.warnings.join("; "));
    }

    for (const auto& e : m_report.entityItems) {
        ss << QString("\"Entity\",\"%1\",\"%2\",%3,%4,%5,%6,%7,\"%8\"\n")
            .arg(e.name)
            .arg(e.relPath)
            .arg(e.instanceCount)
            .arg(e.meshSizeBytes)
            .arg(e.textureRamBytes)
            .arg(e.normalRamBytes + e.specularRamBytes)
            .arg(e.totalTypeRamBytes)
            .arg(e.warnings.join("; "));
    }

    f.close();
    QMessageBox::information(this, QStringLiteral("Export Complete"), QStringLiteral("Memory report saved successfully."));
}
