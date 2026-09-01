#include "MemoryAnalyzerDialog.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

MemoryAnalyzerDialog::MemoryAnalyzerDialog(std::shared_ptr<FPSCMap> map, QWidget* parent)
    : QDialog(parent)
    , m_map(map)
{
    setWindowTitle(QStringLiteral("FPS Creator Entity Memory Footprint Analyzer (MB)"));
    resize(980, 680);
    setMinimumSize(800, 500);

    m_report = MemoryAnalyzer::analyze(m_map);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // 1. KPI Summary Cards
    QHBoxLayout* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(10);

    float totalMb = m_report.totalEstimatedRamBytes / (1024.0f * 1024.0f);
    float meshMb = m_report.totalMeshRamBytes / (1024.0f * 1024.0f);
    float texMb = m_report.totalTextureRamBytes / (1024.0f * 1024.0f);
    float audioMb = m_report.totalAudioRamBytes / (1024.0f * 1024.0f);

    cardsLayout->addWidget(createCard(QStringLiteral("TOTAL ENTITY RAM"), QString("%1 MB").arg(totalMb, 0, 'f', 1), "#3498db"));
    cardsLayout->addWidget(createCard(QStringLiteral("3D MESHES"), QString("%1 MB").arg(meshMb, 0, 'f', 1), "#9b59b6"));
    cardsLayout->addWidget(createCard(QStringLiteral("TEXTURES (RAM)"), QString("%1 MB").arg(texMb, 0, 'f', 1), "#e67e22"));
    cardsLayout->addWidget(createCard(QStringLiteral("AUDIO BUFFERS"), QString("%1 MB").arg(audioMb, 0, 'f', 1), "#1abc9c"));
    cardsLayout->addWidget(createCard(QStringLiteral("PLACED INSTANCES"), QString::number(m_report.totalPlacedEntities), "#f1c40f"));
    mainLayout->addLayout(cardsLayout);

    // 2. Engine Memory Limit Progress Bar
    QGroupBox* gaugeGroup = new QGroupBox(QStringLiteral("32-bit Engine Memory Budget (Limit: ~1850 MB / 1.85 GB)"), this);
    QVBoxLayout* gaugeLayout = new QVBoxLayout(gaugeGroup);

    m_limitLabel = new QLabel(QString("Entity RAM Usage: %1 MB / 1850 MB (%2%) — Status: %3")
        .arg(totalMb, 0, 'f', 1)
        .arg(m_report.engineLimitPercent, 0, 'f', 1)
        .arg(m_report.riskLevel), gaugeGroup);
    m_limitLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    gaugeLayout->addWidget(m_limitLabel);

    m_limitProgress = new QProgressBar(gaugeGroup);
    m_limitProgress->setRange(0, 1850);
    m_limitProgress->setValue(static_cast<int>(totalMb));
    m_limitProgress->setTextVisible(true);
    m_limitProgress->setFormat(QString("%1 MB (%p%)").arg(totalMb, 0, 'f', 1));
    m_limitProgress->setFixedHeight(22);

    QString progressColor = "#2ecc71";
    if (m_report.engineLimitPercent > 75.0f) progressColor = "#e74c3c";
    else if (m_report.engineLimitPercent > 50.0f) progressColor = "#f39c12";

    m_limitProgress->setStyleSheet(QString(
        "QProgressBar { border: 1px solid #3d4455; border-radius: 4px; text-align: center; background: #1a1e28; color: white; }"
        "QProgressBar::chunk { background-color: %1; border-radius: 3px; }"
    ).arg(progressColor));
    gaugeLayout->addWidget(m_limitProgress);

    mainLayout->addWidget(gaugeGroup);

    // 3. Search & Filter
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Filter entity memory table..."));
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(new QLabel(QStringLiteral("Search:"), this));
    searchLayout->addWidget(m_searchEdit, 1);
    mainLayout->addLayout(searchLayout);

    // 4. Detailed Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Icon"),
        QStringLiteral("Entity Profile"),
        QStringLiteral("Category"),
        QStringLiteral("Instances"),
        QStringLiteral("Model (.X)"),
        QStringLiteral("Texture RAM"),
        QStringLiteral("Audio"),
        QStringLiteral("RAM / Inst"),
        QStringLiteral("Total RAM"),
        QStringLiteral("Alerts / Advice")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSortingEnabled(true);
    m_table->setIconSize(QSize(28, 28));
    mainLayout->addWidget(m_table, 1);

    // 5. Optimization Advice Box
    if (!m_report.optimizationTips.isEmpty()) {
        QGroupBox* tipsGroup = new QGroupBox(QStringLiteral("Optimization Recommendations"), this);
        QVBoxLayout* tipsLayout = new QVBoxLayout(tipsGroup);
        m_tipsEdit = new QTextEdit(tipsGroup);
        m_tipsEdit->setReadOnly(true);
        m_tipsEdit->setMaximumHeight(80);
        m_tipsEdit->setStyleSheet("background: #1e222d; color: #f1c40f; border: 1px solid #3d4455; font-size: 11px;");
        m_tipsEdit->setText(m_report.optimizationTips.join("\n• "));
        tipsLayout->addWidget(m_tipsEdit);
        mainLayout->addWidget(tipsGroup);
    }

    // 6. Action Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_copyBtn = new QPushButton(QStringLiteral("Copy Summary to Clipboard"), this);
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

QWidget* MemoryAnalyzerDialog::createCard(const QString& title, const QString& value, const QString& color) {
    QWidget* card = new QWidget(this);
    card->setStyleSheet(QString(
        "background: #1f2430; border: 1px solid #353b4b; border-radius: 6px; padding: 6px;"
    ));
    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(2);

    QLabel* valLbl = new QLabel(value, card);
    valLbl->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(color));
    valLbl->setAlignment(Qt::AlignCenter);
    lay->addWidget(valLbl);

    QLabel* titLbl = new QLabel(title, card);
    titLbl->setStyleSheet("font-size: 10px; color: #8892b0; font-weight: bold;");
    titLbl->setAlignment(Qt::AlignCenter);
    lay->addWidget(titLbl);

    return card;
}

void MemoryAnalyzerDialog::populateUI() {
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    QString filter = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();

    for (int i = 0; i < m_report.items.size(); ++i) {
        const auto& itm = m_report.items[i];

        if (!filter.isEmpty()) {
            bool match = itm.name.toLower().contains(filter) ||
                         itm.relPath.toLower().contains(filter) ||
                         entityCategoryToString(itm.category).toLower().contains(filter);
            if (!match) continue;
        }

        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Col 0: Icon
        QPixmap iconPx;
        if (!itm.iconBmpPath.isEmpty()) iconPx = AssetManager::instance().loadIcon(itm.iconBmpPath);
        QTableWidgetItem* iconItm = new QTableWidgetItem();
        if (!iconPx.isNull()) iconItm->setIcon(QIcon(iconPx));
        m_table->setItem(row, 0, iconItm);

        // Col 1: Name
        QTableWidgetItem* nameItm = new QTableWidgetItem(itm.name);
        nameItm->setToolTip(itm.relPath);
        m_table->setItem(row, 1, nameItm);

        // Col 2: Category
        QTableWidgetItem* catItm = new QTableWidgetItem(entityCategoryToString(itm.category));
        m_table->setItem(row, 2, catItm);

        // Col 3: Instances
        QTableWidgetItem* cntItm = new QTableWidgetItem();
        cntItm->setData(Qt::DisplayRole, itm.instanceCount);
        cntItm->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, cntItm);

        // Col 4: Mesh Size
        QTableWidgetItem* meshItm = new QTableWidgetItem();
        meshItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.meshSizeBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        meshItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 4, meshItm);

        // Col 5: Texture RAM Size
        QTableWidgetItem* texItm = new QTableWidgetItem();
        texItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.textureRamBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        texItm->setToolTip(QString("Resolution: %1x%2\nDisk size: %3 KB")
            .arg(itm.texWidth).arg(itm.texHeight)
            .arg(itm.textureDiskBytes / 1024.0, 0, 'f', 1));
        texItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 5, texItm);

        // Col 6: Audio Size
        QTableWidgetItem* audItm = new QTableWidgetItem();
        audItm->setData(Qt::DisplayRole, QString("%1 KB").arg(itm.audioSizeBytes / 1024.0f, 0, 'f', 1));
        audItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 6, audItm);

        // Col 7: RAM / Inst
        QTableWidgetItem* rpiItm = new QTableWidgetItem();
        rpiItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.ramPerInstanceBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        rpiItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 7, rpiItm);

        // Col 8: Total RAM
        QTableWidgetItem* totItm = new QTableWidgetItem();
        totItm->setData(Qt::DisplayRole, QString("%1 MB").arg(itm.totalTypeRamBytes / (1024.0f * 1024.0f), 0, 'f', 2));
        totItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (itm.totalTypeRamBytes >= 15LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(235, 77, 75));
        } else if (itm.totalTypeRamBytes >= 5LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(241, 196, 15));
        }
        m_table->setItem(row, 8, totItm);

        // Col 9: Alerts
        QTableWidgetItem* alrItm = new QTableWidgetItem(itm.warnings.join(", "));
        if (!itm.warnings.isEmpty()) {
            alrItm->setForeground(QColor(243, 156, 18));
        }
        m_table->setItem(row, 9, alrItm);
    }

    m_table->setSortingEnabled(true);
}

void MemoryAnalyzerDialog::onSearchChanged(const QString&) {
    populateUI();
}

void MemoryAnalyzerDialog::onCopyReport() {
    QString txt;
    QTextStream ss(&txt);
    ss << "=== FPS CREATOR ENTITY MEMORY FOOTPRINT REPORT ===\n";
    ss << "Map: " << (m_map ? m_map->mapName : "Unknown") << "\n";
    ss << "Total Placed Entities: " << m_report.totalPlacedEntities << "\n";
    ss << "Unique Entity Profiles: " << m_report.uniqueEntityTypesCount << "\n";
    ss << "Total Entity RAM: " << (m_report.totalEstimatedRamBytes / (1024.0 * 1024.0)) << " MB\n";
    ss << "  - 3D Meshes: " << (m_report.totalMeshRamBytes / (1024.0 * 1024.0)) << " MB\n";
    ss << "  - Textures: " << (m_report.totalTextureRamBytes / (1024.0 * 1024.0)) << " MB\n";
    ss << "  - Audio: " << (m_report.totalAudioRamBytes / (1024.0 * 1024.0)) << " MB\n";
    ss << "Engine 32-bit Limit Usage: " << m_report.engineLimitPercent << "% (Status: " << m_report.riskLevel << ")\n\n";

    ss << "Top Heavy Entities:\n";
    for (int i = 0; i < qMin(10, m_report.items.size()); ++i) {
        const auto& itm = m_report.items[i];
        ss << QString("  %1. %2 (%3 instances) -> %4 MB [Texture: %5x%6, Model: %7 KB]\n")
            .arg(i + 1)
            .arg(itm.name)
            .arg(itm.instanceCount)
            .arg(itm.totalTypeRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.texWidth).arg(itm.texHeight)
            .arg(itm.meshSizeBytes / 1024.0, 0, 'f', 1);
    }

    if (!m_report.optimizationTips.isEmpty()) {
        ss << "\nOptimization Tips:\n";
        for (const QString& tip : m_report.optimizationTips) {
            ss << "  * " << tip << "\n";
        }
    }

    QApplication::clipboard()->setText(txt);
    QMessageBox::information(this, QStringLiteral("Report Copied"), QStringLiteral("Memory report copied to clipboard."));
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
    ss << "Name,Profile,Category,Instances,MeshBytes,TextureRamBytes,AudioBytes,TotalRamBytes,Alerts\n";
    for (const auto& itm : m_report.items) {
        ss << QString("\"%1\",\"%2\",\"%3\",%4,%5,%6,%7,%8,\"%9\"\n")
            .arg(itm.name)
            .arg(itm.relPath)
            .arg(entityCategoryToString(itm.category))
            .arg(itm.instanceCount)
            .arg(itm.meshSizeBytes)
            .arg(itm.textureRamBytes)
            .arg(itm.audioSizeBytes)
            .arg(itm.totalTypeRamBytes)
            .arg(itm.warnings.join("; "));
    }

    f.close();
    QMessageBox::information(this, QStringLiteral("Export Complete"), QStringLiteral("Memory report saved successfully."));
}
