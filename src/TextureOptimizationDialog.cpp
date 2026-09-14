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

static QString formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

TextureOptimizationDialog::TextureOptimizationDialog(const QString& itemName, const QStringList& targetPaths, QWidget* parent)
    : QDialog(parent)
    , m_itemName(itemName)
{
    setWindowTitle(tr("Texture Optimization Settings — %1").arg(itemName));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(740, 560);

    inspectTargets(targetPaths);
    setupUI();
    loadSavedSettings();
    updateAdviceBanner();
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
        } else if (baseName.endsWith("_s") || baseName.contains("_spec") || baseName.contains("_gloss")) {
            info.semantic = tr("Specular Map");
        } else if (baseName.endsWith("_i") || baseName.contains("_illum") || baseName.contains("_glow")) {
            info.semantic = tr("Illumination");
        } else if (baseName.endsWith("_ao") || baseName.contains("_occl")) {
            info.semantic = tr("Ambient Occlusion");
        } else {
            info.semantic = tr("Diffuse / Color");
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
                        } else if (pfFlags & 0x40) { // DDPF_RGB
                            info.formatStr = QString("RGB%1").arg(bitCount);
                        } else {
                            info.formatStr = "DDS";
                        }
                    }
                }
            }
        } else {
            QImageReader reader(p);
            QSize sz = reader.size();
            info.currentWidth = sz.width();
            info.currentHeight = sz.height();
            info.mipCount = 1;
            info.formatStr = ext.toUpper();
        }

        m_targets.append(info);
    }
}

void TextureOptimizationDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Header Banner
    auto* headerLayout = new QHBoxLayout();
    auto* iconLabel = new QLabel(this);
    iconLabel->setText("⚡");
    QFont iconFont = iconLabel->font();
    iconFont.setPointSize(24);
    iconLabel->setFont(iconFont);

    auto* titleLayout = new QVBoxLayout();
    auto* titleLabel = new QLabel(tr("Optimize Textures — %1").arg(m_itemName), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(tr("Configure DirectX 9 DDS optimization, resolution limits, and mipmap generation:"), this);
    subtitleLabel->setStyleSheet("color: #888888;");

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addWidget(iconLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 2. Target Files Table
    auto* tableGroup = new QGroupBox(tr("Target Textures to Optimize (%1)").arg(m_targets.size()), this);
    auto* tableGroupLayout = new QVBoxLayout(tableGroup);

    m_fileTable = new QTableWidget(this);
    m_fileTable->setColumnCount(6);
    m_fileTable->setHorizontalHeaderLabels({
        tr("Texture File"),
        tr("Role / Type"),
        tr("Resolution"),
        tr("Mips"),
        tr("Disk Size"),
        tr("Current Format")
    });
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->verticalHeader()->setVisible(false);

    m_fileTable->setRowCount(m_targets.size());
    for (int r = 0; r < m_targets.size(); ++r) {
        const auto& t = m_targets[r];

        auto* fileItm = new QTableWidgetItem(QFileInfo(t.filePath).fileName());
        fileItm->setCheckState(t.checked ? Qt::Checked : Qt::Unchecked);
        fileItm->setToolTip(t.filePath);
        m_fileTable->setItem(r, 0, fileItm);

        auto* typeItm = new QTableWidgetItem(t.semantic);
        m_fileTable->setItem(r, 1, typeItm);

        auto* resItm = new QTableWidgetItem(t.currentWidth > 0 ? QString("%1 × %2").arg(t.currentWidth).arg(t.currentHeight) : tr("Unknown"));
        resItm->setTextAlignment(Qt::AlignCenter);
        m_fileTable->setItem(r, 2, resItm);

        auto* mipItm = new QTableWidgetItem(t.mipCount > 0 ? QString::number(t.mipCount) : tr("None"));
        mipItm->setTextAlignment(Qt::AlignCenter);
        m_fileTable->setItem(r, 3, mipItm);

        auto* sizeItm = new QTableWidgetItem(formatFileSize(t.currentSizeBytes));
        sizeItm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_fileTable->setItem(r, 4, sizeItm);

        auto* fmtItm = new QTableWidgetItem(t.formatStr.isEmpty() ? tr("Unknown") : t.formatStr);
        fmtItm->setTextAlignment(Qt::AlignCenter);
        m_fileTable->setItem(r, 5, fmtItm);
    }

    connect(m_fileTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0) {
            int r = item->row();
            if (r >= 0 && r < m_targets.size()) {
                m_targets[r].checked = (item->checkState() == Qt::Checked);
                updateAdviceBanner();
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

    // 3. Preset & Parameters Group
    auto* paramsGroup = new QGroupBox(tr("Optimization Settings"), this);
    auto* paramsLayout = new QGridLayout(paramsGroup);
    paramsLayout->setSpacing(8);

    // Row 0: Preset
    paramsLayout->addWidget(new QLabel(tr("Optimization Preset:"), this), 0, 0);
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(tr("⚖️ Balanced (1024px, Mipmaps, DXT1, Backup) — Recommended"), 0);
    m_presetCombo->addItem(tr("💎 Maximum Quality (2048px, Mipmaps, DXT1, Backup)"), 1);
    m_presetCombo->addItem(tr("⚡ Memory Saver (512px, Mipmaps, DXT1, Backup)"), 2);
    m_presetCombo->addItem(tr("🚀 Ultra-Compact (256px, Mipmaps, DXT1, Backup)"), 3);
    m_presetCombo->addItem(tr("🛠️ Custom Settings"), 4);
    paramsLayout->addWidget(m_presetCombo, 0, 1);

    // Row 1: Max Resolution
    paramsLayout->addWidget(new QLabel(tr("Maximum Resolution:"), this), 1, 0);
    m_maxSizeCombo = new QComboBox(this);
    m_maxSizeCombo->addItem(tr("1024 × 1024 (Balanced — Ideal for game assets)"), 1024);
    m_maxSizeCombo->addItem(tr("2048 × 2048 (HQ — Keep high detail)"), 2048);
    m_maxSizeCombo->addItem(tr("512 × 512 (Medium — Ideal for props)"), 512);
    m_maxSizeCombo->addItem(tr("256 × 256 (Low — Minimal VRAM)"), 256);
    m_maxSizeCombo->addItem(tr("Original / No Limit"), 0);
    paramsLayout->addWidget(m_maxSizeCombo, 1, 1);

    // Row 2: Checkboxes
    m_mipsCheck = new QCheckBox(tr("Generate full Mipmap chain (down to 1×1)"), this);
    m_mipsCheck->setToolTip(tr("Prevents specular shimmering and texture aliasing on distance, optimizes GPU texture cache (+33% VRAM over raw DXT level 0)."));
    paramsLayout->addWidget(m_mipsCheck, 2, 0, 1, 2);

    m_pureAlphaCheck = new QCheckBox(tr("Pure Alpha Check: force opaque textures to DXT1 (4 bpp)"), this);
    m_pureAlphaCheck->setToolTip(tr("Detects opaque textures and compresses them into DXT1 instead of heavy DXT5, saving 50% VRAM."));
    paramsLayout->addWidget(m_pureAlphaCheck, 3, 0, 1, 2);

    m_forcePotCheck = new QCheckBox(tr("Force Power-of-Two (POT) dimensions"), this);
    m_forcePotCheck->setToolTip(tr("Ensures texture dimensions are powers of 2 (256, 512, 1024, 2048), required for legacy DirectX 9 hardware."));
    paramsLayout->addWidget(m_forcePotCheck, 4, 0, 1, 2);

    m_backupCheck = new QCheckBox(tr("Create .bak backup copy before overwriting original files"), this);
    m_backupCheck->setToolTip(tr("Safely preserves the original file so it can be restored anytime."));
    paramsLayout->addWidget(m_backupCheck, 5, 0, 1, 2);

    m_forceCheck = new QCheckBox(tr("Force recompression even if file already appears optimal"), this);
    m_forceCheck->setToolTip(tr("If unchecked, the optimizer will skip textures that already have full mipmaps and compliant D3D9 DXT compression."));
    paramsLayout->addWidget(m_forceCheck, 6, 0, 1, 2);

    mainLayout->addWidget(paramsGroup);

    // 4. Dynamic Advice Banner
    m_adviceLabel = new QLabel(this);
    m_adviceLabel->setWordWrap(true);
    m_adviceLabel->setStyleSheet("background-color: #1e293b; border: 1px solid #334155; border-radius: 4px; padding: 8px; font-size: 11px;");
    mainLayout->addWidget(m_adviceLabel);

    // 5. Action Buttons
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

    // Signal connections
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TextureOptimizationDialog::onPresetChanged);
    connect(m_maxSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TextureOptimizationDialog::onSettingChanged);
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
    updateAdviceBanner();
}

void TextureOptimizationDialog::onPresetChanged(int index) {
    if (index < 0 || index >= 4) return; // Index 4 is Custom

    m_updatingFromPreset = true;
    switch (index) {
    case 0: // Balanced (1024px)
        m_maxSizeCombo->setCurrentIndex(0); // 1024
        m_mipsCheck->setChecked(true);
        m_pureAlphaCheck->setChecked(true);
        m_forcePotCheck->setChecked(true);
        m_backupCheck->setChecked(true);
        m_forceCheck->setChecked(false);
        break;
    case 1: // Maximum Quality (2048px)
        m_maxSizeCombo->setCurrentIndex(1); // 2048
        m_mipsCheck->setChecked(true);
        m_pureAlphaCheck->setChecked(true);
        m_forcePotCheck->setChecked(true);
        m_backupCheck->setChecked(true);
        m_forceCheck->setChecked(false);
        break;
    case 2: // Memory Saver (512px)
        m_maxSizeCombo->setCurrentIndex(2); // 512
        m_mipsCheck->setChecked(true);
        m_pureAlphaCheck->setChecked(true);
        m_forcePotCheck->setChecked(true);
        m_backupCheck->setChecked(true);
        m_forceCheck->setChecked(false);
        break;
    case 3: // Ultra-Compact (256px)
        m_maxSizeCombo->setCurrentIndex(3); // 256
        m_mipsCheck->setChecked(true);
        m_pureAlphaCheck->setChecked(true);
        m_forcePotCheck->setChecked(true);
        m_backupCheck->setChecked(true);
        m_forceCheck->setChecked(false);
        break;
    }
    m_updatingFromPreset = false;
    updateAdviceBanner();
}

void TextureOptimizationDialog::onSettingChanged() {
    if (!m_updatingFromPreset) {
        m_presetCombo->setCurrentIndex(4); // Switch to Custom
    }
    updateAdviceBanner();
}

void TextureOptimizationDialog::updateAdviceBanner() {
    int checkedCount = 0;
    for (const auto& t : m_targets) {
        if (t.checked) checkedCount++;
    }

    if (checkedCount == 0) {
        m_adviceLabel->setText(tr("⚠️ No textures selected. Please select at least one texture to optimize."));
        if (m_okBtn) m_okBtn->setEnabled(false);
        return;
    }

    if (m_okBtn) m_okBtn->setEnabled(true);

    int maxSize = m_maxSizeCombo->currentData().toInt();
    bool mips = m_mipsCheck->isChecked();

    QString msg;
    if (maxSize == 1024) {
        msg = tr("💡 <b>Balanced mode (1024×1024):</b> Reduces 2048×2048 textures by 4× in VRAM (from 2.67 MB to ~0.67 MB) with virtually no visual degradation in game.");
    } else if (maxSize == 2048) {
        msg = tr("💎 <b>Maximum Quality (2048×2048):</b> Preserves original resolution. If a texture is already DXT1 without mipmaps, adding mipmaps will increase disk size by +33% to prevent aliasing and shimmering on distance.");
    } else if (maxSize == 512) {
        msg = tr("⚡ <b>Memory Saver mode (512×512):</b> Drastically reduces VRAM footprint to ~0.17 MB per texture. Ideal for secondary scenery, props, and clutter.");
    } else if (maxSize == 256) {
        msg = tr("🚀 <b>Ultra-Compact mode (256×256):</b> Reduces VRAM to ~0.04 MB per texture. Recommended for low-spec PCs or densely populated levels.");
    } else {
        msg = tr("ℹ️ <b>Original dimensions:</b> Textures will not be resized. They will be compressed to DXT1/DXT5 and normalized.");
    }

    if (!mips) {
        msg += tr("<br><span style='color: #f59e0b;'>⚠️ Mipmaps disabled: Distant objects will experience noisy specular shimmering and texture cache misses on GPU.</span>");
    }

    m_adviceLabel->setText(msg);
}

void TextureOptimizationDialog::loadSavedSettings() {
    QSettings settings("AltitudeEditor", "FPSCMapEditor");
    int preset = settings.value("TexOpt/Preset", 0).toInt();
    if (preset >= 0 && preset < m_presetCombo->count()) {
        m_presetCombo->setCurrentIndex(preset);
    } else {
        m_presetCombo->setCurrentIndex(0);
    }

    // Apply preset values or custom values
    if (preset == 4) { // Custom
        int maxSize = settings.value("TexOpt/MaxSize", 1024).toInt();
        int idx = m_maxSizeCombo->findData(maxSize);
        if (idx >= 0) m_maxSizeCombo->setCurrentIndex(idx);

        m_mipsCheck->setChecked(settings.value("TexOpt/Mips", true).toBool());
        m_pureAlphaCheck->setChecked(settings.value("TexOpt/PureAlpha", true).toBool());
        m_forcePotCheck->setChecked(settings.value("TexOpt/ForcePot", true).toBool());
        m_backupCheck->setChecked(settings.value("TexOpt/Backup", true).toBool());
        m_forceCheck->setChecked(settings.value("TexOpt/Force", false).toBool());
    }
}

void TextureOptimizationDialog::saveSettings() {
    QSettings settings("AltitudeEditor", "FPSCMapEditor");
    settings.setValue("TexOpt/Preset", m_presetCombo->currentIndex());
    settings.setValue("TexOpt/MaxSize", m_maxSizeCombo->currentData().toInt());
    settings.setValue("TexOpt/Mips", m_mipsCheck->isChecked());
    settings.setValue("TexOpt/PureAlpha", m_pureAlphaCheck->isChecked());
    settings.setValue("TexOpt/ForcePot", m_forcePotCheck->isChecked());
    settings.setValue("TexOpt/Backup", m_backupCheck->isChecked());
    settings.setValue("TexOpt/Force", m_forceCheck->isChecked());
}

TextureOptimizationSettings TextureOptimizationDialog::getSettings() const {
    TextureOptimizationSettings s;
    s.maxSize = m_maxSizeCombo->currentData().toInt();
    s.generateMips = m_mipsCheck->isChecked();
    s.pureAlphaCheck = m_pureAlphaCheck->isChecked();
    s.forcePot = m_forcePotCheck->isChecked();
    s.createBackup = m_backupCheck->isChecked();
    s.forceRecompress = m_forceCheck->isChecked();

    for (const auto& t : m_targets) {
        if (t.checked) {
            s.selectedFilePaths.append(t.filePath);
        }
    }
    return s;
}
