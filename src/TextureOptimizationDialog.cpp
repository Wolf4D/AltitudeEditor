#include "TextureOptimizationDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QImageReader>
#include <QFileInfo>
#include <QFile>
#include <QSettings>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QApplication>
#include <algorithm>
#include <cmath>

static QString formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

static qint64 calculateDdsBytes(int w, int h, bool isDxt1, bool mips) {
    if (w <= 0 || h <= 0) return 0;
    qint64 total = 128; // DDS Header
    int curW = w;
    int curH = h;
    while (true) {
        int bw = (curW + 3) / 4;
        int bh = (curH + 3) / 4;
        total += (qint64)bw * bh * (isDxt1 ? 8 : 16);
        if (!mips || (curW == 1 && curH == 1)) break;
        curW = std::max(1, curW / 2);
        curH = std::max(1, curH / 2);
    }
    return total;
}

TextureOptimizationDialog::TextureOptimizationDialog(const QString& itemName, const QStringList& targetPaths, QWidget* parent)
    : QDialog(parent)
    , m_itemName(itemName)
{
    setWindowTitle(tr("Texture Optimization Settings — %1").arg(itemName));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(820, 640);

    inspectTargets(targetPaths);
    setupUI();
    loadSavedSettings();
    updateCalculations();
}

void TextureOptimizationDialog::inspectTargets(const QStringList& paths) {
    m_targets.clear();

    for (const QString& p : paths) {
        if (p.isEmpty() || !QFile::exists(p)) continue;

        TextureTargetInfo info;
        info.filePath = p;
        info.checked = true;

        QFileInfo fi(p);
        info.currentSizeBytes = fi.size();
        QString baseName = fi.completeBaseName().toLower();
        QString ext = fi.suffix().toLower();

        // Determine semantic
        if (baseName.endsWith("_n") || baseName.contains("_norm") || baseName.contains("_bump")) {
            info.semantic = tr("Normal Map");
            info.hasAlpha = false;
        } else if (baseName.endsWith("_s") || baseName.contains("_spec") || baseName.contains("_gloss")) {
            info.semantic = tr("Specular Map");
            info.hasAlpha = false;
        } else if (baseName.endsWith("_i") || baseName.contains("_illum") || baseName.contains("_glow")) {
            info.semantic = tr("Illumination");
            info.hasAlpha = false;
        } else if (baseName.endsWith("_ao") || baseName.contains("_occl")) {
            info.semantic = tr("Ambient Occlusion");
            info.hasAlpha = false;
        } else {
            info.semantic = tr("Diffuse / Color");
            info.hasAlpha = false; // will check header
        }

        // Read dimensions & format
        if (ext == "dds") {
            QFile f(p);
            if (f.open(QIODevice::ReadOnly)) {
                QByteArray hdrData = f.read(128);
                f.close();
                if (hdrData.size() >= 128) {
                    const uint8_t* raw = reinterpret_cast<const uint8_t*>(hdrData.constData());
                    uint32_t magic = *reinterpret_cast<const uint32_t*>(raw);
                    if (magic == 0x20534444) { // "DDS "
                        info.currentHeight = *reinterpret_cast<const uint32_t*>(raw + 12);
                        info.currentWidth = *reinterpret_cast<const uint32_t*>(raw + 16);
                        info.mipCount = *reinterpret_cast<const uint32_t*>(raw + 28);
                        if (info.mipCount <= 0) info.mipCount = 1;

                        uint32_t pfFlags = *reinterpret_cast<const uint32_t*>(raw + 80);
                        uint32_t fourCC = *reinterpret_cast<const uint32_t*>(raw + 84);
                        uint32_t bitCount = *reinterpret_cast<const uint32_t*>(raw + 88);

                        if (pfFlags & 0x04) { // DDPF_FOURCC
                            char fccStr[5] = {0};
                            memcpy(fccStr, &fourCC, 4);
                            info.formatStr = QString::fromLatin1(fccStr).trimmed();
                            if (info.formatStr == "DXT3" || info.formatStr == "DXT5") {
                                info.hasAlpha = true;
                            }
                        } else if (pfFlags & 0x40) { // DDPF_RGB
                            info.formatStr = QString("RGB%1").arg(bitCount);
                            if (bitCount == 32 && (pfFlags & 0x01)) {
                                info.hasAlpha = true;
                            }
                        } else {
                            info.formatStr = "DDS";
                        }
                    }
                }
            }
            info.currentVramBytes = info.currentSizeBytes;
        } else {
            QImageReader reader(p);
            QSize sz = reader.size();
            info.currentWidth = sz.width();
            info.currentHeight = sz.height();
            info.mipCount = 1;
            info.formatStr = ext.toUpper();
            info.hasAlpha = reader.supportsOption(QImageIOHandler::Size) && (ext == "png" || ext == "tga");
            // In Direct3D 9, uncompressed textures expand to 32bpp RGBA in video memory
            info.currentVramBytes = (qint64)info.currentWidth * info.currentHeight * 4;
        }

        info.targetWidth = info.currentWidth;
        info.targetHeight = info.currentHeight;

        m_targets.append(info);
    }
}

QWidget* TextureOptimizationDialog::createStatCard(const QString& title, QLabel*& outValLabel, const QString& accentColor) {
    auto* card = new QWidget(this);
    card->setStyleSheet(QString("background-color: #1e293b; border: 1px solid #334155; border-radius: 6px; padding: 6px;"));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet("color: #94a3b8; font-size: 11px; text-transform: uppercase; font-weight: bold; border: none;");

    outValLabel = new QLabel("—", card);
    outValLabel->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold; border: none;").arg(accentColor));

    layout->addWidget(titleLabel);
    layout->addWidget(outValLabel);
    return card;
}

void TextureOptimizationDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Header
    auto* headerLayout = new QHBoxLayout();
    auto* iconLabel = new QLabel("⚡", this);
    QFont iconFont = iconLabel->font();
    iconFont.setPointSize(24);
    iconLabel->setFont(iconFont);

    auto* titleLayout = new QVBoxLayout();
    auto* titleLabel = new QLabel(tr("Texture Optimization — %1").arg(m_itemName), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(tr("Target resolutions are restricted by each file's current size to prevent upscaling."), this);
    subtitleLabel->setStyleSheet("color: #94a3b8; font-size: 11px;");

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addWidget(iconLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 2. Memory Stats Dashboard (3 Cards: Было, Станет, Экономия)
    auto* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(10);

    statsLayout->addWidget(createStatCard(tr("Current Memory (VRAM)"), m_cardBeforeVal, "#cbd5e1"));
    statsLayout->addWidget(createStatCard(tr("Target Memory (VRAM)"), m_cardAfterVal, "#38bdf8"));
    statsLayout->addWidget(createStatCard(tr("Expected Difference / Saved"), m_cardDeltaVal, "#34d399"));

    mainLayout->addLayout(statsLayout);

    // 3. Target Files Table
    auto* tableGroup = new QGroupBox(tr("Target Textures (%1 files)").arg(m_targets.size()), this);
    auto* tableGroupLayout = new QVBoxLayout(tableGroup);

    m_fileTable = new QTableWidget(this);
    m_fileTable->setColumnCount(7);
    m_fileTable->setHorizontalHeaderLabels({
        tr(""),
        tr("File Name"),
        tr("Role"),
        tr("Current Size"),
        tr("Target Resolution"),
        tr("Current VRAM"),
        tr("Predicted Result")
    });

    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_fileTable->setColumnWidth(0, 28);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->verticalHeader()->setVisible(false);

    m_fileTable->setRowCount(m_targets.size());
    for (int r = 0; r < m_targets.size(); ++r) {
        const auto& t = m_targets[r];

        // Col 0: Checkbox
        auto* checkItm = new QTableWidgetItem();
        checkItm->setCheckState(t.checked ? Qt::Checked : Qt::Unchecked);
        m_fileTable->setItem(r, 0, checkItm);

        // Col 1: File Name
        auto* fileItm = new QTableWidgetItem(QFileInfo(t.filePath).fileName());
        fileItm->setToolTip(t.filePath);
        m_fileTable->setItem(r, 1, fileItm);

        // Col 2: Role
        auto* roleItm = new QTableWidgetItem(t.semantic);
        m_fileTable->setItem(r, 2, roleItm);

        // Col 3: Current Size + Mips
        QString curResStr = QString("%1 × %2 (%3, %4 mips)")
            .arg(t.currentWidth).arg(t.currentHeight)
            .arg(t.formatStr.isEmpty() ? "DDS" : t.formatStr)
            .arg(t.mipCount);
        auto* curItm = new QTableWidgetItem(curResStr);
        curItm->setTextAlignment(Qt::AlignCenter);
        m_fileTable->setItem(r, 3, curItm);

        // Col 4: Target Resolution QComboBox (Strictly limited by current resolution!)
        auto* resCombo = new QComboBox(this);
        int curW = t.currentWidth;
        int curH = t.currentHeight;

        if (curW > 0 && curH > 0) {
            // Option 0: 100% Original
            resCombo->addItem(QString("%1 × %2 (100% — Original)").arg(curW).arg(curH), std::max(curW, curH));

            // Option 1: 50% Half
            if (curW / 2 >= 32 && curH / 2 >= 32) {
                resCombo->addItem(QString("%1 × %2 (50% — Half)").arg(curW / 2).arg(curH / 2), std::max(curW / 2, curH / 2));
            }
            // Option 2: 25% Quarter
            if (curW / 4 >= 32 && curH / 4 >= 32) {
                resCombo->addItem(QString("%1 × %2 (25% — Quarter)").arg(curW / 4).arg(curH / 4), std::max(curW / 4, curH / 4));
            }
            // Option 3: 12.5% Eighth
            if (curW / 8 >= 32 && curH / 8 >= 32) {
                resCombo->addItem(QString("%1 × %2 (12.5% — 1/8)").arg(curW / 8).arg(curH / 8), std::max(curW / 8, curH / 8));
            }
        } else {
            resCombo->addItem(tr("Original (Keep)"), 0);
        }

        connect(resCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, r](int index) {
            onRowResolutionChanged(r, index);
        });

        m_fileTable->setCellWidget(r, 4, resCombo);

        // Col 5: Current VRAM
        auto* vramItm = new QTableWidgetItem(formatFileSize(t.currentVramBytes));
        vramItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_fileTable->setItem(r, 5, vramItm);

        // Col 6: Predicted Result (calculated dynamically)
        auto* predItm = new QTableWidgetItem("—");
        predItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_fileTable->setItem(r, 6, predItm);
    }

    connect(m_fileTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0) {
            int r = item->row();
            if (r >= 0 && r < m_targets.size()) {
                m_targets[r].checked = (item->checkState() == Qt::Checked);
                updateCalculations();
            }
        }
    });

    tableGroupLayout->addWidget(m_fileTable);

    // Quick Select buttons below table
    auto* tableBtnLayout = new QHBoxLayout();
    auto* selAllBtn = new QPushButton(tr("Select All"), this);
    selAllBtn->setMaximumWidth(100);
    auto* deselAllBtn = new QPushButton(tr("Deselect All"), this);
    deselAllBtn->setMaximumWidth(100);
    connect(selAllBtn, &QPushButton::clicked, this, [this]() { onSelectAll(true); });
    connect(deselAllBtn, &QPushButton::clicked, this, [this]() { onSelectAll(false); });
    tableBtnLayout->addWidget(selAllBtn);
    tableBtnLayout->addWidget(deselAllBtn);
    tableBtnLayout->addStretch();
    tableGroupLayout->addLayout(tableBtnLayout);

    mainLayout->addWidget(tableGroup);

    // 4. Global Settings & Controls
    auto* paramsGroup = new QGroupBox(tr("Global Optimization Options"), this);
    auto* paramsLayout = new QGridLayout(paramsGroup);
    paramsLayout->setSpacing(8);

    // Global Scale Combo
    paramsLayout->addWidget(new QLabel(tr("Resolution Preset / Clamp:"), this), 0, 0);
    m_globalScaleCombo = new QComboBox(this);
    m_globalScaleCombo->addItem(tr("⚖️ Clamp to max 1024×1024 (Balanced — Downscales >1024px, keeps smaller textures)"), 1024);
    m_globalScaleCombo->addItem(tr("💎 100% Original Resolution (Keep original dimensions for all textures)"), 0);
    m_globalScaleCombo->addItem(tr("⚡ Downscale All by 50% (Half resolution for all textures)"), -2);
    m_globalScaleCombo->addItem(tr("🚀 Downscale All by 75% (Quarter resolution for all textures)"), -4);
    m_globalScaleCombo->addItem(tr("📦 Clamp to max 512×512 (Memory Saver — Ideal for props and clutter)"), 512);
    m_globalScaleCombo->addItem(tr("🛠️ Custom (Individual settings per file)"), -1);
    paramsLayout->addWidget(m_globalScaleCombo, 0, 1);

    // Checkboxes
    m_mipsCheck = new QCheckBox(tr("Generate complete Mipmap pyramid (down to 1×1)"), this);
    m_mipsCheck->setToolTip(tr("Prevents specular flickering/shimmering on distance and fixes GPU texture cache thrashing (+33% over raw DXT level 0)."));
    paramsLayout->addWidget(m_mipsCheck, 1, 0, 1, 2);

    m_pureAlphaCheck = new QCheckBox(tr("Pure Alpha Check: force opaque textures to DXT1 (4 bpp)"), this);
    m_pureAlphaCheck->setToolTip(tr("Scans alpha channel and encodes opaque textures into DXT1 instead of bulky DXT5, cutting VRAM by 50%."));
    paramsLayout->addWidget(m_pureAlphaCheck, 2, 0, 1, 2);

    m_forcePotCheck = new QCheckBox(tr("Force Power-of-Two (POT: 256, 512, 1024, 2048)"), this);
    m_forcePotCheck->setToolTip(tr("Required for legacy Direct3D 9 hardware texture addressing."));
    paramsLayout->addWidget(m_forcePotCheck, 3, 0, 1, 2);

    m_backupCheck = new QCheckBox(tr("Create .bak backup copies before overwriting original files"), this);
    m_backupCheck->setToolTip(tr("Safely preserves the original file so it can be restored anytime."));
    paramsLayout->addWidget(m_backupCheck, 4, 0, 1, 2);

    m_forceCheck = new QCheckBox(tr("Force recompression even if file already appears optimal"), this);
    m_forceCheck->setToolTip(tr("If unchecked, files with matching DXT format, full mipmaps, and optimal resolution are skipped."));
    paramsLayout->addWidget(m_forceCheck, 5, 0, 1, 2);

    mainLayout->addWidget(paramsGroup);

    // 5. Dynamic Advice Banner
    m_adviceLabel = new QLabel(this);
    m_adviceLabel->setWordWrap(true);
    m_adviceLabel->setStyleSheet("background-color: #1e293b; border: 1px solid #334155; border-radius: 4px; padding: 8px; font-size: 11px;");
    mainLayout->addWidget(m_adviceLabel);

    // 6. Action Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setMinimumWidth(90);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_okBtn = new QPushButton(tr("⚡ Start Optimization"), this);
    m_okBtn->setDefault(true);
    m_okBtn->setMinimumWidth(150);
    m_okBtn->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; border-radius: 4px; padding: 6px 14px; }"
                           "QPushButton:hover { background-color: #0369a1; }"
                           "QPushButton:pressed { background-color: #075985; }");
    connect(m_okBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });

    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_okBtn);
    mainLayout->addLayout(btnLayout);

    // Signals
    connect(m_globalScaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TextureOptimizationDialog::onGlobalScaleChanged);
    connect(m_mipsCheck, &QCheckBox::toggled, this, &TextureOptimizationDialog::onSettingChanged);
    connect(m_pureAlphaCheck, &QCheckBox::toggled, this, &TextureOptimizationDialog::onSettingChanged);
    connect(m_forcePotCheck, &QCheckBox::toggled, this, &TextureOptimizationDialog::onSettingChanged);
    connect(m_backupCheck, &QCheckBox::toggled, this, &TextureOptimizationDialog::onSettingChanged);
    connect(m_forceCheck, &QCheckBox::toggled, this, &TextureOptimizationDialog::onSettingChanged);
}

void TextureOptimizationDialog::onSelectAll(bool select) {
    for (int r = 0; r < m_fileTable->rowCount(); ++r) {
        auto* itm = m_fileTable->item(r, 0);
        if (itm) {
            itm->setCheckState(select ? Qt::Checked : Qt::Unchecked);
        }
        if (r < m_targets.size()) {
            m_targets[r].checked = select;
        }
    }
    updateCalculations();
}

void TextureOptimizationDialog::onRowResolutionChanged(int row, int comboIndex) {
    if (row < 0 || row >= m_targets.size()) return;

    auto* combo = qobject_cast<QComboBox*>(m_fileTable->cellWidget(row, 4));
    if (!combo) return;

    auto& t = m_targets[row];
    int maxDim = combo->currentData().toInt();

    if (maxDim <= 0 || (t.currentWidth <= maxDim && t.currentHeight <= maxDim)) {
        t.targetWidth = t.currentWidth;
        t.targetHeight = t.currentHeight;
    } else {
        float scale = std::min((float)maxDim / t.currentWidth, (float)maxDim / t.currentHeight);
        t.targetWidth = std::max(4, (int)std::round(t.currentWidth * scale));
        t.targetHeight = std::max(4, (int)std::round(t.currentHeight * scale));
    }

    if (!m_updatingFromGlobal) {
        m_globalScaleCombo->setCurrentIndex(5); // Switch to Custom
    }

    updateCalculations();
}

void TextureOptimizationDialog::onGlobalScaleChanged(int index) {
    if (index < 0 || index >= 5) return; // Index 5 is Custom

    m_updatingFromGlobal = true;
    int mode = m_globalScaleCombo->currentData().toInt();

    for (int r = 0; r < m_targets.size(); ++r) {
        auto* combo = qobject_cast<QComboBox*>(m_fileTable->cellWidget(r, 4));
        if (!combo) continue;

        const auto& t = m_targets[r];
        int maxCur = std::max(t.currentWidth, t.currentHeight);

        if (mode == 0) {
            // 100% Original
            combo->setCurrentIndex(0);
        } else if (mode == -2) {
            // 50% Half
            if (combo->count() > 1) combo->setCurrentIndex(1);
            else combo->setCurrentIndex(0);
        } else if (mode == -4) {
            // 25% Quarter
            if (combo->count() > 2) combo->setCurrentIndex(2);
            else if (combo->count() > 1) combo->setCurrentIndex(1);
            else combo->setCurrentIndex(0);
        } else if (mode > 0) {
            // Clamp to mode (e.g. 1024 or 512)
            if (maxCur <= mode) {
                combo->setCurrentIndex(0); // Already within limit, keep original!
            } else {
                // Find closest match <= mode
                int bestIdx = 0;
                for (int ci = 0; ci < combo->count(); ++ci) {
                    int dim = combo->itemData(ci).toInt();
                    if (dim <= mode) {
                        bestIdx = ci;
                        break;
                    }
                }
                combo->setCurrentIndex(bestIdx);
            }
        }
    }

    m_updatingFromGlobal = false;
    updateCalculations();
}

void TextureOptimizationDialog::onSettingChanged() {
    updateCalculations();
}

void TextureOptimizationDialog::updateCalculations() {
    int checkedCount = 0;
    qint64 totalBefore = 0;
    qint64 totalAfter = 0;
    bool generateMips = m_mipsCheck->isChecked();
    bool pureAlpha = m_pureAlphaCheck->isChecked();

    for (int r = 0; r < m_targets.size(); ++r) {
        auto& t = m_targets[r];
        auto* predItm = m_fileTable->item(r, 6);

        if (!t.checked) {
            if (predItm) {
                predItm->setText(tr("Skipped"));
                predItm->setForeground(QColor("#64748b"));
            }
            continue;
        }

        checkedCount++;
        totalBefore += t.currentVramBytes;

        bool isDxt1 = true;
        if (t.hasAlpha && !pureAlpha) {
            isDxt1 = false;
        }

        qint64 targetBytes = calculateDdsBytes(t.targetWidth, t.targetHeight, isDxt1, generateMips);
        totalAfter += targetBytes;

        if (predItm) {
            qint64 rowDiff = targetBytes - t.currentVramBytes;
            double rowPct = t.currentVramBytes > 0 ? ((double)(t.currentVramBytes - targetBytes) / t.currentVramBytes) * 100.0 : 0;

            if (rowDiff < 0) {
                predItm->setText(QString("%1 (-%2%)").arg(formatFileSize(targetBytes)).arg(rowPct, 0, 'f', 1));
                predItm->setForeground(QColor("#34d399")); // Emerald
            } else if (rowDiff > 0) {
                predItm->setText(QString("%1 (+%2%)").arg(formatFileSize(targetBytes)).arg(-rowPct, 0, 'f', 1));
                predItm->setForeground(QColor("#fbbf24")); // Amber
            } else {
                predItm->setText(QString("%1 (0%)").arg(formatFileSize(targetBytes)));
                predItm->setForeground(QColor("#94a3b8"));
            }
        }
    }

    // Update Stat Dashboard
    m_cardBeforeVal->setText(formatFileSize(totalBefore));
    m_cardAfterVal->setText(formatFileSize(totalAfter));

    qint64 delta = totalAfter - totalBefore;
    if (delta < 0) {
        qint64 saved = -delta;
        double pct = totalBefore > 0 ? ((double)saved / totalBefore) * 100.0 : 0.0;
        m_cardDeltaVal->setText(QString("-%1 (-%2%)").arg(formatFileSize(saved)).arg(pct, 0, 'f', 1));
        m_cardDeltaVal->setStyleSheet("color: #34d399; font-size: 18px; font-weight: bold; border: none;");
    } else if (delta > 0) {
        double pct = totalBefore > 0 ? ((double)delta / totalBefore) * 100.0 : 0.0;
        m_cardDeltaVal->setText(QString("+%1 (+%2%)").arg(formatFileSize(delta)).arg(pct, 0, 'f', 1));
        m_cardDeltaVal->setStyleSheet("color: #fbbf24; font-size: 18px; font-weight: bold; border: none;");
    } else {
        m_cardDeltaVal->setText("0 B (0.0%)");
        m_cardDeltaVal->setStyleSheet("color: #94a3b8; font-size: 18px; font-weight: bold; border: none;");
    }

    if (checkedCount == 0) {
        m_adviceLabel->setText(tr("⚠️ No textures selected. Please select at least one texture to optimize."));
        if (m_okBtn) m_okBtn->setEnabled(false);
        return;
    }

    if (m_okBtn) m_okBtn->setEnabled(true);

    QString advice;
    if (delta < 0) {
        advice = tr("💡 <b>High VRAM Savings:</b> Selected settings will free up <b>%1</b> of video memory while maintaining clean DirectX 9 DDS compliance.")
            .arg(formatFileSize(-delta));
    } else if (delta > 0) {
        advice = tr("ℹ️ <b>Resolution Preserved + Mipmaps Added:</b> Target memory will increase by <b>+%1</b> because previously un-mipmapped textures are receiving complete mip pyramids to prevent sparkling/aliasing on distance.")
            .arg(formatFileSize(delta));
    } else {
        advice = tr("ℹ️ Textures will be normalized to legacy Direct3D 9 DDS format without size changes.");
    }

    if (!generateMips) {
        advice += tr("<br><span style='color: #f59e0b;'>⚠️ Mipmaps disabled: Distant objects will experience noisy specular shimmering and texture cache misses on GPU.</span>");
    }

    m_adviceLabel->setText(advice);
}

void TextureOptimizationDialog::loadSavedSettings() {
    QSettings settings("AltitudeEditor", "FPSCMapEditor");
    int scaleIdx = settings.value("TexOpt/GlobalScaleIndex", 0).toInt();
    if (scaleIdx >= 0 && scaleIdx < m_globalScaleCombo->count()) {
        m_globalScaleCombo->setCurrentIndex(scaleIdx);
    } else {
        m_globalScaleCombo->setCurrentIndex(0); // 1024 clamp by default
    }

    m_mipsCheck->setChecked(settings.value("TexOpt/Mips", true).toBool());
    m_pureAlphaCheck->setChecked(settings.value("TexOpt/PureAlpha", true).toBool());
    m_forcePotCheck->setChecked(settings.value("TexOpt/ForcePot", true).toBool());
    m_backupCheck->setChecked(settings.value("TexOpt/Backup", true).toBool());
    m_forceCheck->setChecked(settings.value("TexOpt/Force", false).toBool());

    onGlobalScaleChanged(m_globalScaleCombo->currentIndex());
}

void TextureOptimizationDialog::saveSettings() {
    QSettings settings("AltitudeEditor", "FPSCMapEditor");
    settings.setValue("TexOpt/GlobalScaleIndex", m_globalScaleCombo->currentIndex());
    settings.setValue("TexOpt/Mips", m_mipsCheck->isChecked());
    settings.setValue("TexOpt/PureAlpha", m_pureAlphaCheck->isChecked());
    settings.setValue("TexOpt/ForcePot", m_forcePotCheck->isChecked());
    settings.setValue("TexOpt/Backup", m_backupCheck->isChecked());
    settings.setValue("TexOpt/Force", m_forceCheck->isChecked());
}

TextureOptimizationSettings TextureOptimizationDialog::getSettings() const {
    TextureOptimizationSettings s;
    s.generateMips = m_mipsCheck->isChecked();
    s.pureAlphaCheck = m_pureAlphaCheck->isChecked();
    s.forcePot = m_forcePotCheck->isChecked();
    s.createBackup = m_backupCheck->isChecked();
    s.forceRecompress = m_forceCheck->isChecked();

    for (int r = 0; r < m_targets.size(); ++r) {
        const auto& t = m_targets[r];
        if (!t.checked) continue;

        TextureTargetTask task;
        task.filePath = t.filePath;
        task.targetMaxSize = std::max(t.targetWidth, t.targetHeight);
        s.tasks.append(task);
    }

    return s;
}
