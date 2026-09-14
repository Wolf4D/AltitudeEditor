#include "MemoryAnalyzerDialog.h"
#include "Version.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QMenu>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QSettings>

class NumericTableWidgetItem : public QTableWidgetItem {
public:
    NumericTableWidgetItem(const QString& text, double sortVal)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, sortVal);
    }
    NumericTableWidgetItem(const QString& text, qint64 sortVal)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, static_cast<double>(sortVal));
    }
    NumericTableWidgetItem(const QString& text, int sortVal)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, static_cast<double>(sortVal));
    }

    bool operator<(const QTableWidgetItem& other) const override {
        QVariant v1 = data(Qt::UserRole);
        QVariant v2 = other.data(Qt::UserRole);
        if (v1.isValid() && v2.isValid()) {
            return v1.toDouble() < v2.toDouble();
        }
        return QTableWidgetItem::operator<(other);
    }
};

MemoryAnalyzerDialog::MemoryAnalyzerDialog(std::shared_ptr<FPSCMap> map, const MemoryReport* cachedReport, QWidget* parent)
    : QDialog(parent)
    , m_map(map)
{
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 Memory Footprint Analyzer — %2").arg(VersionInfo::shortTitle(), mapName));
    resize(1160, 740);
    setMinimumSize(880, 540);

    if (cachedReport) {
        m_report = *cachedReport;
        m_isCalculating = false;
    } else if (parent == nullptr) {
        m_report = MemoryAnalyzer::analyze(m_map);
        m_isCalculating = false;
    } else {
        m_isCalculating = true;
    }

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // 1. KPI Summary Cards
    QHBoxLayout* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(8);

    cardsLayout->addWidget(createCard(tr("TOTAL LEVEL RAM"), m_cardTotalVal, m_cardTotalTitle, "#3498db"));
    cardsLayout->addWidget(createCard(tr("SEGMENTS (ROOMS)"), m_cardSegVal, m_cardSegTitle, "#e67e22"));
    cardsLayout->addWidget(createCard(tr("ENTITIES (PROPS)"), m_cardEntVal, m_cardEntTitle, "#9b59b6"));
    cardsLayout->addWidget(createCard(tr("UNIVERSE & LIGHTMAPS"), m_cardUniVal, m_cardUniTitle, "#1abc9c"));
    cardsLayout->addWidget(createCard(tr("ENGINE & D3D BASE"), m_cardEngineVal, m_cardEngineTitle, "#f1c40f"));
    mainLayout->addLayout(cardsLayout);

    // 2. Engine Memory Limit Progress Bar
    m_gaugeGroup = new QGroupBox(tr("DirectX 9 / 32-bit Process Memory Budget (Limit: ~1850 MB)"), this);
    QVBoxLayout* gaugeLayout = new QVBoxLayout(m_gaugeGroup);
    gaugeLayout->setContentsMargins(10, 8, 10, 8);
    gaugeLayout->setSpacing(4);

    m_limitLabel = new QLabel(m_gaugeGroup);
    m_limitLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    gaugeLayout->addWidget(m_limitLabel);

    m_limitProgress = new QProgressBar(m_gaugeGroup);
    m_limitProgress->setRange(0, 1850);
    m_limitProgress->setTextVisible(true);
    m_limitProgress->setFixedHeight(22);
    gaugeLayout->addWidget(m_limitProgress);

    mainLayout->addWidget(m_gaugeGroup);

    // 3. Search & Filter
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter table by name, category, or texture path..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchLabel = new QLabel(tr("Search:"), this);
    searchLayout->addWidget(m_searchLabel);
    searchLayout->addWidget(m_searchEdit, 1);
    mainLayout->addLayout(searchLayout);

    // 4. Tab Widget
    m_tabWidget = new QTabWidget(this);

    // Tab 1: Entities Table
    m_entityTable = new QTableWidget(this);
    m_entityTable->setObjectName("entityTable");
    m_entityTable->setColumnCount(10);
    m_entityTable->setHorizontalHeaderLabels({
        tr("Icon"),
        tr("Entity Profile"),
        tr("Category"),
        tr("Placed"),
        tr("Model (.X)"),
        tr("Texture RAM"),
        tr("Audio"),
        tr("RAM / Inst"),
        tr("Total RAM"),
        tr("Alerts / Advice")
    });
    m_entityTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_entityTable->horizontalHeader()->setMinimumSectionSize(30);
    m_entityTable->horizontalHeader()->setStretchLastSection(true);
    m_entityTable->setColumnWidth(0, 46);
    m_entityTable->setColumnWidth(1, 220);
    m_entityTable->setColumnWidth(2, 115);
    m_entityTable->setColumnWidth(3, 65);
    m_entityTable->setColumnWidth(4, 90);
    m_entityTable->setColumnWidth(5, 95);
    m_entityTable->setColumnWidth(6, 75);
    m_entityTable->setColumnWidth(7, 85);
    m_entityTable->setColumnWidth(8, 90);
    m_entityTable->setColumnWidth(9, 220);
    m_entityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_entityTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_entityTable->setAlternatingRowColors(true);
    m_entityTable->verticalHeader()->setVisible(false);
    m_entityTable->setIconSize(QSize(28, 28));
    m_tabWidget->addTab(m_entityTable, tr("Entities"));

    // Tab 2: Segments Table
    m_segmentTable = new QTableWidget(this);
    m_segmentTable->setObjectName("segmentTable");
    m_segmentTable->setColumnCount(9);
    m_segmentTable->setHorizontalHeaderLabels({
        tr("Icon"),
        tr("Segment Name"),
        tr("Parts"),
        tr("Placed Blocks"),
        tr("Mesh RAM"),
        tr("Diffuse RAM"),
        tr("Normal/Spec RAM"),
        tr("Total RAM"),
        tr("Alerts")
    });
    m_segmentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_segmentTable->horizontalHeader()->setMinimumSectionSize(30);
    m_segmentTable->horizontalHeader()->setStretchLastSection(true);
    m_segmentTable->setColumnWidth(0, 46);
    m_segmentTable->setColumnWidth(1, 230);
    m_segmentTable->setColumnWidth(2, 60);
    m_segmentTable->setColumnWidth(3, 95);
    m_segmentTable->setColumnWidth(4, 90);
    m_segmentTable->setColumnWidth(5, 95);
    m_segmentTable->setColumnWidth(6, 120);
    m_segmentTable->setColumnWidth(7, 95);
    m_segmentTable->setColumnWidth(8, 220);
    m_segmentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_segmentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_segmentTable->setAlternatingRowColors(true);
    m_segmentTable->verticalHeader()->setVisible(false);
    m_segmentTable->setIconSize(QSize(28, 28));
    m_tabWidget->addTab(m_segmentTable, tr("Segments (Architecture)"));

    // Tab 3: Universe & Engine Breakdown Table
    m_engineTable = new QTableWidget(this);
    m_engineTable->setObjectName("engineTable");
    m_engineTable->setColumnCount(4);
    m_engineTable->setHorizontalHeaderLabels({
        tr("Component"),
        tr("Type / Format"),
        tr("Estimated RAM"),
        tr("Description")
    });
    m_engineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_engineTable->horizontalHeader()->setMinimumSectionSize(30);
    m_engineTable->horizontalHeader()->setStretchLastSection(true);
    m_engineTable->setColumnWidth(0, 240);
    m_engineTable->setColumnWidth(1, 140);
    m_engineTable->setColumnWidth(2, 120);
    m_engineTable->setColumnWidth(3, 300);
    m_engineTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_engineTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_engineTable->setAlternatingRowColors(true);
    m_engineTable->verticalHeader()->setVisible(false);
    m_tabWidget->addTab(m_engineTable, tr("Universe & Engine Breakdown"));

    // Tab 4: Optimization Recommendations
    m_tipsTab = new QWidget(this);
    QVBoxLayout* tipsLayout = new QVBoxLayout(m_tipsTab);
    m_tipsEdit = new QTextEdit(m_tipsTab);
    m_tipsEdit->setReadOnly(true);
    m_tipsEdit->setStyleSheet("background: #1e222d; color: #f1c40f; border: 1px solid #3d4455; font-size: 12px; font-family: monospace;");
    tipsLayout->addWidget(m_tipsEdit);
    m_tabWidget->addTab(m_tipsTab, tr("Optimization Tips"));

    mainLayout->addWidget(m_tabWidget, 1);

    // 5. Action Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_copyBtn = new QPushButton(tr("Copy Full Report to Clipboard"), this);
    m_exportBtn = new QPushButton(tr("Export CSV Report..."), this);
    m_optBtn = new QPushButton(tr("⚡ Optimize Selected Textures..."), this);
    m_optBtn->setStyleSheet("QPushButton { background-color: #0f766e; color: #ccfbf1; font-weight: bold; } QPushButton:hover { background-color: #115e59; color: #ffffff; }");
    m_optBtn->setToolTip(tr("Run FPSC Texture Optimizer on the selected entity or segment textures"));
    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setDefault(true);

    btnLayout->addWidget(m_copyBtn);
    btnLayout->addWidget(m_exportBtn);
    btnLayout->addWidget(m_optBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &MemoryAnalyzerDialog::onSearchChanged);
    connect(m_copyBtn, &QPushButton::clicked, this, &MemoryAnalyzerDialog::onCopyReport);
    connect(m_exportBtn, &QPushButton::clicked, this, &MemoryAnalyzerDialog::onExportCSV);
    connect(m_optBtn, &QPushButton::clicked, this, &MemoryAnalyzerDialog::onOptimizeSelected);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_entityTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (row >= 0 && row < m_entityTable->rowCount()) {
            QTableWidgetItem* nameItm = m_entityTable->item(row, 1);
            QString contextPath = nameItm ? nameItm->data(Qt::UserRole).toString() : QString();

            QTableWidgetItem* clickedItm = (col >= 0) ? m_entityTable->item(row, col) : nullptr;
            QString targetPath = clickedItm ? clickedItm->data(Qt::UserRole).toString() : QString();
            if (targetPath.isEmpty()) targetPath = contextPath;

            AssetManager::showInExplorer(targetPath, contextPath);
        }
    });

    m_entityTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_entityTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        int row = m_entityTable->rowAt(pos.y());
        int col = m_entityTable->columnAt(pos.x());
        if (row < 0 || row >= m_entityTable->rowCount()) return;

        QTableWidgetItem* nameItm = m_entityTable->item(row, 1);
        if (!nameItm) return;
        QString entPath = nameItm->data(Qt::UserRole).toString();
        QString fullEntPath = AssetManager::instance().resolvePath(entPath);

        QTableWidgetItem* clickedItm = (col >= 0) ? m_entityTable->item(row, col) : nullptr;
        QString clickedPath = clickedItm ? clickedItm->data(Qt::UserRole).toString() : QString();
        QString fullClickedPath = !clickedPath.isEmpty() ? AssetManager::instance().resolvePath(clickedPath, entPath) : QString();

        QTableWidgetItem* meshItm = m_entityTable->item(row, 4);
        QString modelPath = meshItm ? meshItm->data(Qt::UserRole).toString() : QString();
        QString fullModelPath = !modelPath.isEmpty() ? AssetManager::instance().resolvePath(modelPath, entPath) : QString();

        QTableWidgetItem* texItm = m_entityTable->item(row, 5);
        QString texPath = texItm ? texItm->data(Qt::UserRole).toString() : QString();
        QString fullTexPath = !texPath.isEmpty() ? AssetManager::instance().resolvePath(texPath, entPath) : QString();

        QTableWidgetItem* audItm = m_entityTable->item(row, 6);
        QString audPath = audItm ? audItm->data(Qt::UserRole).toString() : QString();
        QString fullAudPath = !audPath.isEmpty() ? AssetManager::instance().resolvePath(audPath, entPath) : QString();

        QMenu menu(this);

        if (!fullClickedPath.isEmpty() && fullClickedPath != fullEntPath) {
            QString cLabel = tr("📁 Show \"%1\" in Explorer...").arg(QFileInfo(fullClickedPath).fileName());
            QAction* actClicked = menu.addAction(cLabel);
            QFont f = actClicked->font();
            f.setBold(true);
            actClicked->setFont(f);
            menu.addSeparator();
        }

        QAction* actEnt = nullptr;
        if (!fullEntPath.isEmpty()) {
            actEnt = menu.addAction(tr("📁 Show Entity Profile (\"%1\") in Explorer...").arg(QFileInfo(fullEntPath).fileName()));
        }
        QAction* actModel = nullptr;
        if (!fullModelPath.isEmpty() && fullModelPath != fullEntPath && fullModelPath != fullClickedPath) {
            actModel = menu.addAction(tr("📁 Show Model (\"%1\") in Explorer...").arg(QFileInfo(fullModelPath).fileName()));
        }
        QAction* actTex = nullptr;
        if (!fullTexPath.isEmpty() && fullTexPath != fullEntPath && fullTexPath != fullClickedPath) {
            actTex = menu.addAction(tr("📁 Show Texture (\"%1\") in Explorer...").arg(QFileInfo(fullTexPath).fileName()));
        }
        QAction* actAud = nullptr;
        if (!fullAudPath.isEmpty() && fullAudPath != fullEntPath && fullAudPath != fullClickedPath) {
            actAud = menu.addAction(tr("📁 Show Audio (\"%1\") in Explorer...").arg(QFileInfo(fullAudPath).fileName()));
        }

        menu.addSeparator();
        QString entDir = QFileInfo(fullEntPath).absolutePath();
        QAction* actOptTex = nullptr;
        if (!fullTexPath.isEmpty()) {
            actOptTex = menu.addAction(tr("⚡ Optimize Texture (\"%1\") [DXT/Mips]...").arg(QFileInfo(fullTexPath).fileName()));
        }
        QAction* actOptDir = nullptr;
        if (!entDir.isEmpty() && QDir(entDir).exists()) {
            actOptDir = menu.addAction(tr("⚡ Optimize All Textures in Entity Folder..."));
        }

        QAction* chosen = menu.exec(m_entityTable->viewport()->mapToGlobal(pos));
        if (!chosen) return;

        if (chosen == actOptTex && !fullTexPath.isEmpty()) {
            optimizeTarget(fullTexPath, QFileInfo(fullTexPath).fileName());
        } else if (chosen == actOptDir && !entDir.isEmpty()) {
            optimizeTarget(entDir, QFileInfo(entDir).fileName() + " (Folder)");
        } else if (chosen == actEnt) {
            AssetManager::showInExplorer(fullEntPath, entPath);
        } else if (chosen == actModel) {
            AssetManager::showInExplorer(fullModelPath, entPath);
        } else if (chosen == actTex) {
            AssetManager::showInExplorer(fullTexPath, entPath);
        } else if (chosen == actAud) {
            AssetManager::showInExplorer(fullAudPath, entPath);
        } else if (!fullClickedPath.isEmpty()) {
            AssetManager::showInExplorer(fullClickedPath, entPath);
        }
    });

    connect(m_segmentTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (row >= 0 && row < m_segmentTable->rowCount()) {
            QTableWidgetItem* nameItm = m_segmentTable->item(row, 1);
            QString contextPath = nameItm ? nameItm->data(Qt::UserRole).toString() : QString();

            QTableWidgetItem* clickedItm = (col >= 0) ? m_segmentTable->item(row, col) : nullptr;
            QString targetPath = clickedItm ? clickedItm->data(Qt::UserRole).toString() : QString();
            if (targetPath.isEmpty()) targetPath = contextPath;

            AssetManager::showInExplorer(targetPath, contextPath);
        }
    });

    m_segmentTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_segmentTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        int row = m_segmentTable->rowAt(pos.y());
        int col = m_segmentTable->columnAt(pos.x());
        if (row < 0 || row >= m_segmentTable->rowCount()) return;

        QTableWidgetItem* nameItm = m_segmentTable->item(row, 1);
        if (!nameItm) return;
        QString segPath = nameItm->data(Qt::UserRole).toString();
        QString fullSegPath = AssetManager::instance().resolvePath(segPath);

        QTableWidgetItem* clickedItm = (col >= 0) ? m_segmentTable->item(row, col) : nullptr;
        QString clickedPath = clickedItm ? clickedItm->data(Qt::UserRole).toString() : QString();
        QString fullClickedPath = !clickedPath.isEmpty() ? AssetManager::instance().resolvePath(clickedPath, segPath) : QString();

        QTableWidgetItem* meshItm = m_segmentTable->item(row, 4);
        QString meshPath = meshItm ? meshItm->data(Qt::UserRole).toString() : QString();
        QString fullMeshPath = !meshPath.isEmpty() ? AssetManager::instance().resolvePath(meshPath, segPath) : QString();

        QTableWidgetItem* diffItm = m_segmentTable->item(row, 5);
        QString diffPath = diffItm ? diffItm->data(Qt::UserRole).toString() : QString();
        QString fullDiffPath = !diffPath.isEmpty() ? AssetManager::instance().resolvePath(diffPath, segPath) : QString();

        QTableWidgetItem* nsItm = m_segmentTable->item(row, 6);
        QString nsPath = nsItm ? nsItm->data(Qt::UserRole).toString() : QString();
        QString fullNsPath = !nsPath.isEmpty() ? AssetManager::instance().resolvePath(nsPath, segPath) : QString();

        QMenu menu(this);

        if (!fullClickedPath.isEmpty() && fullClickedPath != fullSegPath) {
            QString cLabel = tr("📁 Show \"%1\" in Explorer...").arg(QFileInfo(fullClickedPath).fileName());
            QAction* actClicked = menu.addAction(cLabel);
            QFont f = actClicked->font();
            f.setBold(true);
            actClicked->setFont(f);
            menu.addSeparator();
        }

        QAction* actSeg = nullptr;
        if (!fullSegPath.isEmpty()) {
            actSeg = menu.addAction(tr("📁 Show Segment Profile (\"%1\") in Explorer...").arg(QFileInfo(fullSegPath).fileName()));
        }
        QAction* actMesh = nullptr;
        if (!fullMeshPath.isEmpty() && fullMeshPath != fullSegPath && fullMeshPath != fullClickedPath) {
            actMesh = menu.addAction(tr("📁 Show Mesh (\"%1\") in Explorer...").arg(QFileInfo(fullMeshPath).fileName()));
        }
        QAction* actDiff = nullptr;
        if (!fullDiffPath.isEmpty() && fullDiffPath != fullSegPath && fullDiffPath != fullClickedPath) {
            actDiff = menu.addAction(tr("📁 Show Diffuse Texture (\"%1\") in Explorer...").arg(QFileInfo(fullDiffPath).fileName()));
        }
        QAction* actNs = nullptr;
        if (!fullNsPath.isEmpty() && fullNsPath != fullSegPath && fullNsPath != fullClickedPath && fullNsPath != fullDiffPath) {
            actNs = menu.addAction(tr("📁 Show Normal/Spec Texture (\"%1\") in Explorer...").arg(QFileInfo(fullNsPath).fileName()));
        }

        menu.addSeparator();
        QString segDir = QFileInfo(fullSegPath).absolutePath();
        QAction* actOptDiff = nullptr;
        if (!fullDiffPath.isEmpty()) {
            actOptDiff = menu.addAction(tr("⚡ Optimize Diffuse Texture (\"%1\") [DXT/Mips]...").arg(QFileInfo(fullDiffPath).fileName()));
        }
        QAction* actOptNs = nullptr;
        if (!fullNsPath.isEmpty()) {
            actOptNs = menu.addAction(tr("⚡ Optimize Normal/Spec Texture (\"%1\") [DXT/Mips]...").arg(QFileInfo(fullNsPath).fileName()));
        }
        QAction* actOptSegDir = nullptr;
        if (!segDir.isEmpty() && QDir(segDir).exists()) {
            actOptSegDir = menu.addAction(tr("⚡ Optimize All Textures in Segment Folder..."));
        }

        QAction* chosen = menu.exec(m_segmentTable->viewport()->mapToGlobal(pos));
        if (!chosen) return;

        if (chosen == actOptDiff && !fullDiffPath.isEmpty()) {
            optimizeTarget(fullDiffPath, QFileInfo(fullDiffPath).fileName());
        } else if (chosen == actOptNs && !fullNsPath.isEmpty()) {
            optimizeTarget(fullNsPath, QFileInfo(fullNsPath).fileName());
        } else if (chosen == actOptSegDir && !segDir.isEmpty()) {
            optimizeTarget(segDir, QFileInfo(segDir).fileName() + " (Folder)");
        } else if (chosen == actSeg) {
            AssetManager::showInExplorer(fullSegPath, segPath);
        } else if (chosen == actMesh) {
            AssetManager::showInExplorer(fullMeshPath, segPath);
        } else if (chosen == actDiff) {
            AssetManager::showInExplorer(fullDiffPath, segPath);
        } else if (chosen == actNs) {
            AssetManager::showInExplorer(fullNsPath, segPath);
        } else if (!fullClickedPath.isEmpty()) {
            AssetManager::showInExplorer(fullClickedPath, segPath);
        }
    });

    connect(m_engineTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        if (row >= 0 && row < m_engineTable->rowCount()) {
            QTableWidgetItem* itm = m_engineTable->item(row, 0);
            if (itm) {
                QString path = itm->data(Qt::UserRole).toString();
                if (!path.isEmpty()) {
                    AssetManager::showInExplorer(path);
                }
            }
        }
    });

    populateUI();
}

void MemoryAnalyzerDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void MemoryAnalyzerDialog::retranslateUi() {
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 Memory Footprint Analyzer — %2").arg(VersionInfo::shortTitle(), mapName));

    if (m_cardTotalTitle) m_cardTotalTitle->setText(tr("TOTAL LEVEL RAM"));
    if (m_cardSegTitle) m_cardSegTitle->setText(tr("SEGMENTS (ROOMS)"));
    if (m_cardEntTitle) m_cardEntTitle->setText(tr("ENTITIES (PROPS)"));
    if (m_cardUniTitle) m_cardUniTitle->setText(tr("UNIVERSE & LIGHTMAPS"));
    if (m_cardEngineTitle) m_cardEngineTitle->setText(tr("ENGINE & D3D BASE"));

    if (m_gaugeGroup) m_gaugeGroup->setTitle(tr("DirectX 9 / 32-bit Process Memory Budget (Limit: ~1850 MB)"));
    if (m_searchLabel) m_searchLabel->setText(tr("Search:"));
    if (m_searchEdit) m_searchEdit->setPlaceholderText(tr("Filter table by name, category, or texture path..."));

    if (m_entityTable) {
        m_entityTable->setHorizontalHeaderLabels({
            tr("Icon"),
            tr("Entity Profile"),
            tr("Category"),
            tr("Placed"),
            tr("Model (.X)"),
            tr("Texture RAM"),
            tr("Audio"),
            tr("RAM / Inst"),
            tr("Total RAM"),
            tr("Alerts / Advice")
        });
    }

    if (m_segmentTable) {
        m_segmentTable->setHorizontalHeaderLabels({
            tr("Icon"),
            tr("Segment Name"),
            tr("Parts"),
            tr("Placed Blocks"),
            tr("Mesh RAM"),
            tr("Diffuse RAM"),
            tr("Normal/Spec RAM"),
            tr("Total RAM"),
            tr("Alerts")
        });
    }

    if (m_engineTable) {
        m_engineTable->setHorizontalHeaderLabels({
            tr("Component"),
            tr("Type / Format"),
            tr("Estimated RAM"),
            tr("Description")
        });
    }

    if (m_copyBtn) m_copyBtn->setText(tr("Copy Full Report to Clipboard"));
    if (m_exportBtn) m_exportBtn->setText(tr("Export CSV Report..."));
    if (m_optBtn) m_optBtn->setText(tr("⚡ Optimize Selected Textures..."));
    if (m_closeBtn) m_closeBtn->setText(tr("Close"));

    if (m_isCalculating) {
        showLoadingState();
    } else {
        populateUI();
    }
}

void MemoryAnalyzerDialog::setMap(std::shared_ptr<FPSCMap> map, const MemoryReport* cachedReport) {
    m_map = map;
    QString mapName = m_map ? m_map->mapName : tr("No Map");
    setWindowTitle(tr("%1 Memory Footprint Analyzer — %2").arg(VersionInfo::shortTitle(), mapName));
    if (cachedReport) {
        setReport(*cachedReport);
    } else {
        showLoadingState();
    }
}

void MemoryAnalyzerDialog::setReport(const MemoryReport& report) {
    m_report = report;
    m_isCalculating = false;
    populateUI();
}

void MemoryAnalyzerDialog::showLoadingState() {
    m_isCalculating = true;

    if (m_cardTotalVal) m_cardTotalVal->setText(tr("..."));
    if (m_cardSegVal) m_cardSegVal->setText(tr("..."));
    if (m_cardEntVal) m_cardEntVal->setText(tr("..."));
    if (m_cardUniVal) m_cardUniVal->setText(tr("..."));
    if (m_cardEngineVal) m_cardEngineVal->setText(tr("..."));

    if (m_limitLabel) {
        m_limitLabel->setText(tr("Level RAM Usage: Calculating memory footprint..."));
    }

    if (m_limitProgress) {
        m_limitProgress->setRange(0, 0);
        m_limitProgress->setFormat(tr("Calculating memory footprint..."));
        m_limitProgress->setStyleSheet(
            "QProgressBar { border: 1px solid #4a5568; border-radius: 4px; text-align: center; background-color: #141720; color: #ffffff; font-size: 11px; font-weight: bold; height: 22px; }"
            "QProgressBar::chunk { background-color: #3498db; border-radius: 3px; }"
        );
    }

    if (m_tabWidget) {
        m_tabWidget->setTabText(0, tr("Entities (calculating...)"));
        m_tabWidget->setTabText(1, tr("Segments (calculating...)"));
    }

    if (m_entityTable) m_entityTable->setRowCount(0);
    if (m_segmentTable) m_segmentTable->setRowCount(0);
    if (m_engineTable) m_engineTable->setRowCount(0);

    if (m_tipsEdit) {
        m_tipsEdit->setPlainText(tr("Calculating level RAM memory footprint in background..."));
    }
}

QWidget* MemoryAnalyzerDialog::createCard(const QString& title, QLabel*& outValueLabel, QLabel*& outTitleLabel, const QString& color) {
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
        m_limitLabel->setText(tr("Level RAM Usage: %1 MB / 1850 MB (%2%) — Status: %3")
            .arg(totalMb, 0, 'f', 1)
            .arg(m_report.engineLimitPercent, 0, 'f', 1)
            .arg(m_report.riskLevel));
    }

    if (m_limitProgress) {
        int limitMb = 1850;
        int clampedVal = qBound(0, static_cast<int>(totalMb), limitMb);
        m_limitProgress->setRange(0, limitMb);
        m_limitProgress->setValue(clampedVal);

        if (totalMb > limitMb) {
            m_limitProgress->setFormat(tr("%1 MB / %2 MB (%3%) — OVER BUDGET!")
                .arg(totalMb, 0, 'f', 1)
                .arg(limitMb)
                .arg(m_report.engineLimitPercent, 0, 'f', 1));
        } else {
            m_limitProgress->setFormat(tr("%1 MB / %2 MB (%3%)")
                .arg(totalMb, 0, 'f', 1)
                .arg(limitMb)
                .arg(m_report.engineLimitPercent, 0, 'f', 1));
        }

        QString progressColor = "#2ecc71";
        if (m_report.engineLimitPercent >= 90.0f) progressColor = "#e74c3c";
        else if (m_report.engineLimitPercent >= 70.0f) progressColor = "#f39c12";

        m_limitProgress->setStyleSheet(QString(
            "QProgressBar { border: 1px solid #4a5568; border-radius: 4px; text-align: center; background-color: #141720; color: #ffffff; font-size: 11px; font-weight: bold; height: 22px; }"
            "QProgressBar::chunk { background-color: %1; border-radius: 3px; }"
        ).arg(progressColor));
    }

    // Update tab titles with counts
    m_tabWidget->setTabText(0, tr("Entities (%1 types, %2 placed)").arg(m_report.uniqueEntityTypesCount).arg(m_report.totalPlacedEntities));
    m_tabWidget->setTabText(1, tr("Segments (%1 types, %2 blocks)").arg(m_report.uniqueSegmentTypesCount).arg(m_report.totalPlacedSegmentBlocks));

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
        iconItm->setData(Qt::UserRole, itm.relPath);
        m_entityTable->setItem(row, 0, iconItm);

        // Col 1: Name
        QTableWidgetItem* nameItm = new QTableWidgetItem(itm.name);
        nameItm->setData(Qt::UserRole, itm.relPath);
        nameItm->setToolTip(itm.relPath + tr("\nDouble-click to reveal in Windows Explorer"));
        m_entityTable->setItem(row, 1, nameItm);

        // Col 2: Category
        QTableWidgetItem* catItm = new QTableWidgetItem(entityCategoryToString(itm.category));
        catItm->setData(Qt::UserRole, itm.relPath);
        m_entityTable->setItem(row, 2, catItm);

        // Col 3: Instances
        QTableWidgetItem* cntItm = new NumericTableWidgetItem(QString::number(itm.instanceCount), itm.instanceCount);
        cntItm->setTextAlignment(Qt::AlignCenter);
        cntItm->setData(Qt::UserRole, itm.relPath);
        m_entityTable->setItem(row, 3, cntItm);

        // Col 4: Mesh Size
        QTableWidgetItem* meshItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(itm.meshSizeBytes / (1024.0f * 1024.0f), 0, 'f', 2),
            itm.meshSizeBytes
        );
        meshItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        meshItm->setData(Qt::UserRole, itm.modelPath.isEmpty() ? itm.relPath : itm.modelPath);
        meshItm->setToolTip(QString("Model: %1\nDouble-click to reveal in Windows Explorer")
            .arg(itm.modelPath.isEmpty() ? tr("(None)") : itm.modelPath));
        m_entityTable->setItem(row, 4, meshItm);

        // Col 5: Texture RAM Size
        qint64 texTotal = itm.textureRamBytes + itm.normalRamBytes + itm.specularRamBytes;
        QTableWidgetItem* texItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(texTotal / (1024.0f * 1024.0f), 0, 'f', 2),
            texTotal
        );
        texItm->setData(Qt::UserRole, itm.texturePath.isEmpty() ? itm.relPath : itm.texturePath);
        texItm->setToolTip(QString("Diffuse: %1 MB (%2)\nNormals: %3 MB (%4)\nSpecular: %5 MB (%6)\nRes: %7x%8\nDouble-click to reveal in Windows Explorer")
            .arg(itm.textureRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.texturePath.isEmpty() ? tr("None") : itm.texturePath)
            .arg(itm.normalRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.normalPath.isEmpty() ? tr("None") : itm.normalPath)
            .arg(itm.specularRamBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(itm.specularPath.isEmpty() ? tr("None") : itm.specularPath)
            .arg(itm.texWidth).arg(itm.texHeight));
        texItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_entityTable->setItem(row, 5, texItm);

        // Col 6: Audio Size
        QTableWidgetItem* audItm = new NumericTableWidgetItem(
            QString("%1 KB").arg(itm.audioSizeBytes / 1024.0f, 0, 'f', 1),
            itm.audioSizeBytes
        );
        audItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        audItm->setData(Qt::UserRole, itm.audioPath.isEmpty() ? itm.relPath : itm.audioPath);
        if (!itm.audioPath.isEmpty()) {
            audItm->setToolTip(QString("Audio: %1\nDouble-click to reveal in Windows Explorer").arg(itm.audioPath));
        }
        m_entityTable->setItem(row, 6, audItm);

        // Col 7: RAM / Inst
        QTableWidgetItem* rpiItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(itm.ramPerInstanceBytes / (1024.0f * 1024.0f), 0, 'f', 2),
            itm.ramPerInstanceBytes
        );
        rpiItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rpiItm->setData(Qt::UserRole, itm.relPath);
        m_entityTable->setItem(row, 7, rpiItm);

        // Col 8: Total RAM
        QTableWidgetItem* totItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(itm.totalTypeRamBytes / (1024.0f * 1024.0f), 0, 'f', 2),
            itm.totalTypeRamBytes
        );
        totItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        totItm->setData(Qt::UserRole, itm.relPath);
        if (itm.totalTypeRamBytes >= 15LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(235, 77, 75));
        } else if (itm.totalTypeRamBytes >= 5LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(241, 196, 15));
        }
        m_entityTable->setItem(row, 8, totItm);

        // Col 9: Alerts
        QTableWidgetItem* alrItm = new QTableWidgetItem(itm.warnings.join(", "));
        alrItm->setData(Qt::UserRole, itm.relPath);
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
        iconItm->setData(Qt::UserRole, itm.relPath);
        m_segmentTable->setItem(row, 0, iconItm);

        // Col 1: Name
        QTableWidgetItem* nameItm = new QTableWidgetItem(itm.name);
        nameItm->setData(Qt::UserRole, itm.relPath);
        nameItm->setToolTip(itm.relPath + tr("\nDouble-click to reveal in Windows Explorer"));
        m_segmentTable->setItem(row, 1, nameItm);

        // Col 2: Parts
        QTableWidgetItem* partsItm = new NumericTableWidgetItem(QString::number(itm.partCount), itm.partCount);
        partsItm->setTextAlignment(Qt::AlignCenter);
        partsItm->setData(Qt::UserRole, itm.relPath);
        m_segmentTable->setItem(row, 2, partsItm);

        // Col 3: Placed Blocks
        QTableWidgetItem* placedItm = new NumericTableWidgetItem(QString::number(itm.placedCount), itm.placedCount);
        placedItm->setTextAlignment(Qt::AlignCenter);
        placedItm->setData(Qt::UserRole, itm.relPath);
        m_segmentTable->setItem(row, 3, placedItm);

        // Col 4: Mesh RAM
        QTableWidgetItem* meshItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(itm.meshSizeBytes / (1024.0f * 1024.0f), 0, 'f', 2),
            itm.meshSizeBytes
        );
        meshItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        meshItm->setData(Qt::UserRole, itm.meshPath.isEmpty() ? itm.relPath : itm.meshPath);
        meshItm->setToolTip(QString("Mesh: %1 (%2 parts)\nDouble-click to reveal in Windows Explorer")
            .arg(itm.meshPath.isEmpty() ? tr("(Built-in)") : itm.meshPath)
            .arg(itm.uniqueMeshCount));
        m_segmentTable->setItem(row, 4, meshItm);

        // Col 5: Diffuse RAM
        QTableWidgetItem* diffItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(itm.diffuseRamBytes / (1024.0f * 1024.0f), 0, 'f', 2),
            itm.diffuseRamBytes
        );
        diffItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        diffItm->setData(Qt::UserRole, itm.diffusePath.isEmpty() ? itm.relPath : itm.diffusePath);
        diffItm->setToolTip(QString("Diffuse Texture: %1\nDouble-click to reveal in Windows Explorer")
            .arg(itm.diffusePath.isEmpty() ? tr("(None)") : itm.diffusePath));
        m_segmentTable->setItem(row, 5, diffItm);

        // Col 6: Normal/Spec RAM
        qint64 normSpec = itm.normalRamBytes + itm.specularRamBytes;
        QTableWidgetItem* nsItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(normSpec / (1024.0f * 1024.0f), 0, 'f', 2),
            normSpec
        );
        nsItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QString nsPath = !itm.normalPath.isEmpty() ? itm.normalPath : itm.specularPath;
        nsItm->setData(Qt::UserRole, nsPath.isEmpty() ? (itm.diffusePath.isEmpty() ? itm.relPath : itm.diffusePath) : nsPath);
        nsItm->setToolTip(QString("Normal Map: %1\nSpecular Map: %2\nDouble-click to reveal in Windows Explorer")
            .arg(itm.normalPath.isEmpty() ? tr("None") : itm.normalPath)
            .arg(itm.specularPath.isEmpty() ? tr("None") : itm.specularPath));
        m_segmentTable->setItem(row, 6, nsItm);

        // Col 7: Total RAM
        QTableWidgetItem* totItm = new NumericTableWidgetItem(
            QString("%1 MB").arg(itm.totalTypeRamBytes / (1024.0f * 1024.0f), 0, 'f', 2),
            itm.totalTypeRamBytes
        );
        totItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        totItm->setData(Qt::UserRole, itm.relPath);
        if (itm.totalTypeRamBytes >= 15LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(235, 77, 75));
        } else if (itm.totalTypeRamBytes >= 5LL * 1024LL * 1024LL) {
            totItm->setForeground(QColor(241, 196, 15));
        }
        m_segmentTable->setItem(row, 7, totItm);

        // Col 8: Alerts
        QTableWidgetItem* alrItm = new QTableWidgetItem(itm.warnings.join(", "));
        alrItm->setData(Qt::UserRole, itm.relPath);
        if (!itm.warnings.isEmpty()) {
            alrItm->setForeground(QColor(243, 156, 18));
        }
        m_segmentTable->setItem(row, 8, alrItm);
    }
    m_segmentTable->setSortingEnabled(true);

    // 3. Populate Universe & Engine Breakdown Table
    m_engineTable->setSortingEnabled(false);
    m_engineTable->setRowCount(0);
    auto addEngineRow = [&](const QString& comp, const QString& type, float mb, const QString& desc, const QString& fileTarget = QString()) {
        int r = m_engineTable->rowCount();
        m_engineTable->insertRow(r);
        QTableWidgetItem* compItm = new QTableWidgetItem(comp);
        if (!fileTarget.isEmpty()) {
            compItm->setData(Qt::UserRole, fileTarget);
            compItm->setToolTip(fileTarget + tr("\nDouble-click to reveal in Windows Explorer"));
        }
        m_engineTable->setItem(r, 0, compItm);
        m_engineTable->setItem(r, 1, new QTableWidgetItem(type));
        QTableWidgetItem* ramItm = new NumericTableWidgetItem(QString("%1 MB").arg(mb, 0, 'f', 1), static_cast<double>(mb));
        ramItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_engineTable->setItem(r, 2, ramItm);
        m_engineTable->setItem(r, 3, new QTableWidgetItem(desc));
    };

    QString universeDbo = AssetManager::instance().resolvePath(QStringLiteral("universe.dbo"));
    if (universeDbo.isEmpty()) universeDbo = AssetManager::instance().resolvePath(QStringLiteral("universe.dbu"));

    addEngineRow(QStringLiteral("Universe CSG Geometry"), QStringLiteral("universe.dbo"),
                 m_report.universeCsgRamBytes / (1024.0f * 1024.0f),
                 QStringLiteral("Compiled CSG BSP tree, portal connectivity graphs, and static world trimeshes"),
                 universeDbo);

    addEngineRow(QStringLiteral("Level Lightmaps"), QStringLiteral("D3D Surfaces"),
                 m_report.lightmapsRamBytes / (1024.0f * 1024.0f),
                 QStringLiteral("Pre-calculated static radiance lightmaps baked across all sector surfaces"));

    addEngineRow(QStringLiteral("D3D9 Render Targets & Buffers"), QStringLiteral("D3DSURFACE9"),
                 40.0f,
                 QStringLiteral("1080p Backbuffer, 32-bit Depth/Stencil Buffer, Dynamic Shadow Map, Post-FX Bloom Buffers"));

    addEngineRow(QStringLiteral("Engine & Physics DLLs"), QStringLiteral("Runtime Code"),
                 25.0f,
                 QStringLiteral("FPSC-Game.exe core, DarkBasic Pro runtime modules, ODE physics engine, DirectSound mixer"));

    m_engineTable->setSortingEnabled(true);

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

void MemoryAnalyzerDialog::onOptimizeSelected() {
    int currentTab = m_tabWidget ? m_tabWidget->currentIndex() : 0;
    if (currentTab == 0 && m_entityTable) { // Entities
        int row = m_entityTable->currentRow();
        if (row < 0 || row >= m_entityTable->rowCount()) {
            QMessageBox::information(this, tr("No Selection"), tr("Please select an entity row from the table first."));
            return;
        }
        QTableWidgetItem* nameItm = m_entityTable->item(row, 1);
        if (!nameItm) return;
        QString entPath = nameItm->data(Qt::UserRole).toString();
        QString fullEntPath = AssetManager::instance().resolvePath(entPath);
        QString entDir = QFileInfo(fullEntPath).absolutePath();
        if (!entDir.isEmpty() && QDir(entDir).exists()) {
            optimizeTarget(entDir, nameItm->text());
        }
    } else if (currentTab == 1 && m_segmentTable) { // Segments
        int row = m_segmentTable->currentRow();
        if (row < 0 || row >= m_segmentTable->rowCount()) {
            QMessageBox::information(this, tr("No Selection"), tr("Please select a segment row from the table first."));
            return;
        }
        QTableWidgetItem* nameItm = m_segmentTable->item(row, 1);
        if (!nameItm) return;
        QString segPath = nameItm->data(Qt::UserRole).toString();
        QString fullSegPath = AssetManager::instance().resolvePath(segPath);
        QString segDir = QFileInfo(fullSegPath).absolutePath();
        if (!segDir.isEmpty() && QDir(segDir).exists()) {
            optimizeTarget(segDir, nameItm->text());
        }
    } else {
        QMessageBox::information(this, tr("No Selection"), tr("Please select an entity or segment row to optimize."));
    }
}

void MemoryAnalyzerDialog::optimizeTarget(const QString& targetPath, const QString& itemName) {
    if (targetPath.isEmpty()) return;

    // 1. Locate optimizer executable
    QString exePath;
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/fpsc_texopt.exe",
        appDir + "/texopt.exe",
        "C:/FPSC_TexOptimizer/build/fpsc_texopt.exe",
        "C:/FPSC_TexOptimizer/build/texopt.exe",
        QStandardPaths::findExecutable("fpsc_texopt.exe"),
        QStandardPaths::findExecutable("texopt.exe")
    };

    QSettings settings("AltitudeEditor", "FPSCMapEditor");
    QString savedExe = settings.value("Paths/TexOptimizer").toString();
    if (!savedExe.isEmpty() && QFile::exists(savedExe)) {
        candidates.prepend(savedExe);
    }

    for (const auto& cand : candidates) {
        if (!cand.isEmpty() && QFile::exists(cand)) {
            exePath = cand;
            break;
        }
    }

    if (exePath.isEmpty()) {
        QMessageBox::StandardButton btn = QMessageBox::question(
            this,
            tr("FPSC Texture Optimizer Not Found"),
            tr("The texture optimizer utility (fpsc_texopt.exe) was not found in the application directory.\n\n"
               "Would you like to locate it manually?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (btn == QMessageBox::Yes) {
            exePath = QFileDialog::getOpenFileName(this, tr("Locate fpsc_texopt.exe"), QString(), "Executables (*.exe)");
            if (!exePath.isEmpty() && QFile::exists(exePath)) {
                settings.setValue("Paths/TexOptimizer", exePath);
            } else {
                return;
            }
        } else {
            return;
        }
    }

    // 2. Confirmation prompt
    QMessageBox::StandardButton confirm = QMessageBox::question(
        this,
        tr("Optimize Textures — %1").arg(itemName),
        tr("Run FPSC Texture Optimizer on:\n%1\n\n"
           "Actions performed:\n"
           "• Automatic .bak backup copy creation before overwrite\n"
           "• Opaque textures forced to DXT1 (50% VRAM savings)\n"
           "• Mipmap pyramid generated down to 1x1\n"
           "• Strict DirectX 9 legacy DDS compliance (no DX10 headers)\n\n"
           "Proceed with optimization?").arg(targetPath),
        QMessageBox::Yes | QMessageBox::Cancel
    );

    if (confirm != QMessageBox::Yes) return;

    // 3. Execution with modal progress dialog
    QProgressDialog progress(tr("Optimizing textures with FPSC_TexOptimizer..."), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.show();
    QApplication::processEvents();

    QProcess process;
    QStringList args;
    args << "-i" << targetPath << "-b" << "--max-size" << "2048" << "-V";
    process.start(exePath, args);
    process.waitForFinished(60000); // 60s timeout

    progress.close();

    int exitCode = process.exitCode();
    QString stdoutStr = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString stderrStr = QString::fromLocal8Bit(process.readAllStandardError());

    if (exitCode == 0) {
        // Clear asset caches so updated textures and metrics reload freshly
        AssetManager::instance().clearCache();
        showLoadingState();
        emit requestReanalysis();

        QMessageBox::information(
            this,
            tr("Optimization Complete"),
            tr("Texture optimization finished successfully!\n\n%1").arg(stdoutStr.trimmed())
        );
    } else {
        QMessageBox::warning(
            this,
            tr("Optimization Failed"),
            tr("Texture optimizer exited with error code %1:\n\n%2\n%3")
                .arg(exitCode).arg(stdoutStr).arg(stderrStr)
        );
    }
}
