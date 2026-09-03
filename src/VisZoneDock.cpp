#include "VisZoneDock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <QHeaderView>

VisZoneDock::VisZoneDock(QWidget* parent)
    : QDockWidget(QStringLiteral("Visibility Zones & Portals (PVS)"), parent)
{
    setObjectName("VisZoneDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // Elegant Dark Reddish / Crimson UI Theme
    setStyleSheet(QStringLiteral(
        "QDockWidget#VisZoneDock {"
        "  color: #e0d2d4;"
        "  font-family: 'Segoe UI', sans-serif;"
        "}"
        "QDockWidget#VisZoneDock::title {"
        "  background: #331c21;"
        "  color: #f2b6be;"
        "  font-weight: bold;"
        "  border-bottom: 1px solid #522831;"
        "  padding: 6px;"
        "  text-align: left;"
        "}"
        "QWidget#VisZoneCentral {"
        "  background-color: #1e1518;"
        "}"
        "QGroupBox {"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  color: #e57373;"
        "  border: 1px solid #4a272e;"
        "  border-radius: 4px;"
        "  margin-top: 10px;"
        "  padding-top: 8px;"
        "  background-color: #24191c;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 8px;"
        "  padding: 0 4px;"
        "  color: #e57373;"
        "  background-color: #24191c;"
        "}"
        "QComboBox, QSpinBox, QLineEdit {"
        "  background: #1a1214;"
        "  border: 1px solid #4a272e;"
        "  border-radius: 4px;"
        "  padding: 0px 6px;"
        "  height: 26px;"
        "  min-height: 26px;"
        "  max-height: 26px;"
        "  color: #f0d5d8;"
        "}"
        "QComboBox:hover {"
        "  border-color: #a93226;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #1a1214;"
        "  color: #f0d5d8;"
        "  selection-background-color: #5c2028;"
        "  selection-color: #ffffff;"
        "  border: 1px solid #4a272e;"
        "}"
        "QPushButton {"
        "  background: #2e1b1f;"
        "  border: 1px solid #4a2b32;"
        "  border-radius: 4px;"
        "  padding: 0px 10px;"
        "  height: 26px;"
        "  min-height: 26px;"
        "  max-height: 26px;"
        "  color: #f0d5d8;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: #3e242a;"
        "  border-color: #6b3e48;"
        "  color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "  background: #1e1215;"
        "}"
        "QPushButton#btnResetZones {"
        "  background-color: #3b1b21;"
        "  color: #ff6e80;"
        "  border: 1px solid #5a262f;"
        "  border-radius: 4px;"
        "  padding: 0px 8px;"
        "  height: 28px;"
        "  min-height: 28px;"
        "  max-height: 28px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#btnResetZones:hover {"
        "  background-color: #4a2028;"
        "  color: #ff8fa0;"
        "  border-color: #78313e;"
        "}"
        "QCheckBox, QRadioButton {"
        "  color: #d8c2c5;"
        "  spacing: 6px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "  border: 1px solid #5a3138;"
        "  border-radius: 3px;"
        "  background: #1a1214;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: #c0392b;"
        "  border-color: #e74c3c;"
        "}"
        "QRadioButton::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "  border: 1px solid #5a3138;"
        "  border-radius: 7px;"
        "  background: #1a1214;"
        "}"
        "QRadioButton::indicator:checked {"
        "  background: #c0392b;"
        "  border-color: #e74c3c;"
        "}"
        "QSlider::groove:horizontal {"
        "  height: 6px;"
        "  background: #2a1a1d;"
        "  border-radius: 3px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #a93226;"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #e74c3c;"
        "  border: 1px solid #ffffff;"
        "  width: 14px;"
        "  margin: -4px 0;"
        "  border-radius: 7px;"
        "}"
        "QListWidget {"
        "  background-color: #171012;"
        "  border: 1px solid #3d2127;"
        "  border-radius: 4px;"
        "  color: #e0d0d2;"
        "  padding: 2px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #5c2028;"
        "  color: #ffffff;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #2b161a;"
        "}"
        "QLabel {"
        "  color: #d8c2c5;"
        "}"
    ));

    QWidget* central = new QWidget(this);
    central->setObjectName("VisZoneCentral");
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. Zone Selection Group
    QGroupBox* grpSelection = new QGroupBox(QStringLiteral("Zone Selection"), central);
    QVBoxLayout* selLayout = new QVBoxLayout(grpSelection);
    selLayout->setSpacing(6);

    m_chkCurrentFloorOnly = new QCheckBox(QStringLiteral("Filter to Current Floor"), grpSelection);
    m_chkCurrentFloorOnly->setChecked(true);
    selLayout->addWidget(m_chkCurrentFloorOnly);

    QHBoxLayout* colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    m_chkColorAll = new QCheckBox(QStringLiteral("🎨 Покрасить все зоны"), grpSelection);
    m_chkColorAll->setToolTip(QStringLiteral("Отображать цветовую карту всех виз-зон на этаже одновременно (Ctrl+Shift+C)"));
    m_btnRecolor = new QPushButton(QStringLiteral("🎲 Палитра"), grpSelection);
    m_btnRecolor->setToolTip(QStringLiteral("Перегенерировать случайную палитру цветов для всех зон"));
    m_btnRecolor->setFixedWidth(90);
    colorRow->addWidget(m_chkColorAll, 1);
    colorRow->addWidget(m_btnRecolor, 0);
    selLayout->addLayout(colorRow);

    connect(m_chkColorAll, &QCheckBox::toggled, this, [this](bool checked) {
        emit colorAllZonesToggled(checked);
    });
    connect(m_btnRecolor, &QPushButton::clicked, this, [this]() {
        if (m_mgr) {
            static int s_hueShift = 0;
            s_hueShift = (s_hueShift + 67) % 360;
            m_mgr->recolorAllZones(s_hueShift);
            if (!m_chkColorAll->isChecked()) {
                m_chkColorAll->setChecked(true);
            } else {
                emit colorAllZonesToggled(true);
            }
        }
    });

    m_zoneCombo = new QComboBox(grpSelection);
    selLayout->addWidget(m_zoneCombo);

    QHBoxLayout* navLayout = new QHBoxLayout();
    m_btnPrev = new QPushButton(QStringLiteral("< Prev Zone"), grpSelection);
    m_btnNext = new QPushButton(QStringLiteral("Next Zone >"), grpSelection);
    navLayout->addWidget(m_btnPrev);
    navLayout->addWidget(m_btnNext);
    selLayout->addLayout(navLayout);

    QPushButton* btnReset = new QPushButton(QStringLiteral("Show All Zones (Normal View)"), grpSelection);
    btnReset->setObjectName("btnResetZones");
    btnReset->setToolTip(QStringLiteral("Reset map display to show all zones and segments"));
    selLayout->addWidget(btnReset);
    connect(btnReset, &QPushButton::clicked, this, &VisZoneDock::resetToNormalView);

    mainLayout->addWidget(grpSelection);

    // 2. Isolation / Culling Controls Group
    QGroupBox* grpIsolation = new QGroupBox(QStringLiteral("Visibility Culling Mode"), central);
    QVBoxLayout* isoLayout = new QVBoxLayout(grpIsolation);
    isoLayout->setSpacing(6);

    m_chkIsolate = new QCheckBox(QStringLiteral("Isolate Active Zone"), grpIsolation);
    m_chkIsolate->setChecked(true);
    isoLayout->addWidget(m_chkIsolate);

    QHBoxLayout* radioLayout = new QHBoxLayout();
    m_radioHide = new QRadioButton(QStringLiteral("Hide Outside"), grpIsolation);
    m_radioHide->setChecked(true);
    m_radioDim = new QRadioButton(QStringLiteral("Dim Outside (Ghost)"), grpIsolation);
    radioLayout->addWidget(m_radioHide);
    radioLayout->addWidget(m_radioDim);
    isoLayout->addLayout(radioLayout);

    QHBoxLayout* dimLayout = new QHBoxLayout();
    dimLayout->addWidget(new QLabel(QStringLiteral("Dim:"), grpIsolation));
    m_sliderDim = new QSlider(Qt::Horizontal, grpIsolation);
    m_sliderDim->setRange(5, 50);
    m_sliderDim->setValue(15);
    m_sliderDim->setEnabled(false);
    dimLayout->addWidget(m_sliderDim);
    isoLayout->addLayout(dimLayout);

    mainLayout->addWidget(grpIsolation);

    // 3. Active Zone Info
    QGroupBox* grpDetails = new QGroupBox(QStringLiteral("Active Zone Details"), central);
    QVBoxLayout* detLayout = new QVBoxLayout(grpDetails);
    detLayout->setSpacing(4);

    m_lblStats = new QLabel(QStringLiteral("All zones visible. No single zone isolated."), grpDetails);
    m_lblStats->setWordWrap(true);
    m_lblStats->setStyleSheet(QStringLiteral("background-color: #1a1012; border: 1px solid #4a2026; border-radius: 4px; padding: 6px; color: #ff8a80; font-weight: bold;"));
    detLayout->addWidget(m_lblStats);

    QLabel* lblPortals = new QLabel(QStringLiteral("Connected Portals (double-click to jump):"), grpDetails);
    lblPortals->setStyleSheet(QStringLiteral("color: #d8a0a6; font-weight: bold; font-size: 11px;"));
    detLayout->addWidget(lblPortals);
    m_listPortals = new QListWidget(grpDetails);
    m_listPortals->setMaximumHeight(90);
    detLayout->addWidget(m_listPortals);

    QLabel* lblEntities = new QLabel(QStringLiteral("Contained Entities (click to inspect):"), grpDetails);
    lblEntities->setStyleSheet(QStringLiteral("color: #d8a0a6; font-weight: bold; font-size: 11px;"));
    detLayout->addWidget(lblEntities);
    m_listEntities = new QListWidget(grpDetails);
    detLayout->addWidget(m_listEntities);

    mainLayout->addWidget(grpDetails, 1);

    setWidget(central);

    // Connections
    connect(m_chkCurrentFloorOnly, &QCheckBox::toggled, this, &VisZoneDock::onFloorFilterToggled);
    connect(m_zoneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisZoneDock::onZoneComboChanged);
    connect(m_btnPrev, &QPushButton::clicked, this, &VisZoneDock::onPrevZone);
    connect(m_btnNext, &QPushButton::clicked, this, &VisZoneDock::onNextZone);

    connect(m_chkIsolate, &QCheckBox::toggled, this, &VisZoneDock::onIsolationOptionChanged);
    connect(m_radioHide, &QRadioButton::toggled, this, [this](bool checked) {
        m_sliderDim->setEnabled(!checked && m_chkIsolate->isChecked());
        onIsolationOptionChanged();
    });
    connect(m_radioDim, &QRadioButton::toggled, this, [this](bool checked) {
        m_sliderDim->setEnabled(checked && m_chkIsolate->isChecked());
        onIsolationOptionChanged();
    });
    connect(m_sliderDim, &QSlider::valueChanged, this, &VisZoneDock::onIsolationOptionChanged);

    connect(m_listPortals, &QListWidget::itemDoubleClicked, this, &VisZoneDock::onPortalDoubleClicked);
    connect(m_listEntities, &QListWidget::itemClicked, this, &VisZoneDock::onEntityClicked);
}

void VisZoneDock::setVisZoneManager(std::shared_ptr<VisZoneManager> mgr) {
    m_mgr = mgr;
    populateZoneCombo();
    updateActiveZoneDetails();
}

void VisZoneDock::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    populateZoneCombo();
    updateActiveZoneDetails();
}

void VisZoneDock::onFloorChanged(int floor) {
    m_currentFloor = floor;
    if (m_chkCurrentFloorOnly->isChecked()) {
        populateZoneCombo();
    }
}

void VisZoneDock::onFloorFilterToggled(bool /*checked*/) {
    populateZoneCombo();
}

void VisZoneDock::populateZoneCombo() {
    m_updatingCombo = true;
    m_zoneCombo->clear();

    m_zoneCombo->addItem(QStringLiteral("All Zones (Normal View)"), -1);

    if (!m_mgr) {
        m_updatingCombo = false;
        return;
    }

    bool floorOnly = m_chkCurrentFloorOnly->isChecked();
    const auto& zones = m_mgr->zones();

    int selectedIdx = 0;
    for (size_t i = 0; i < zones.size(); ++i) {
        const auto& z = zones[i];
        if (floorOnly && !z.hasFloor(m_currentFloor)) continue;

        QString floorStr = (z.minFloor == z.maxFloor)
                           ? QString("Floor %1").arg(z.floor)
                           : QString("Floors %1..%2").arg(z.minFloor).arg(z.maxFloor);

        QString label = QString("Zone %1 (%2: %3 tiles, %4 entities, %5 portals)")
                        .arg(z.id + 1)
                        .arg(floorStr)
                        .arg(z.tiles.size())
                        .arg(z.entityIndices.size())
                        .arg(z.portalIndices.size());

        m_zoneCombo->addItem(label, z.id);
        if (z.id == m_activeZoneId) {
            selectedIdx = m_zoneCombo->count() - 1;
        }
    }

    m_zoneCombo->setCurrentIndex(selectedIdx);
    m_updatingCombo = false;
}

void VisZoneDock::onZoneComboChanged(int index) {
    if (m_updatingCombo || index < 0) return;

    int zoneId = m_zoneCombo->itemData(index).toInt();
    m_activeZoneId = zoneId;

    if (zoneId >= 0 && m_chkIsolate && !m_chkIsolate->isChecked()) {
        m_chkIsolate->blockSignals(true);
        m_chkIsolate->setChecked(true);
        m_chkIsolate->blockSignals(false);
    }

    updateActiveZoneDetails();
    emit zoneSelected(zoneId);
    onIsolationOptionChanged();
}

void VisZoneDock::onPrevZone() {
    int cur = m_zoneCombo->currentIndex();
    if (cur > 0) {
        m_zoneCombo->setCurrentIndex(cur - 1);
    } else if (m_zoneCombo->count() > 1) {
        m_zoneCombo->setCurrentIndex(m_zoneCombo->count() - 1);
    }
}

void VisZoneDock::onNextZone() {
    int cur = m_zoneCombo->currentIndex();
    if (cur < m_zoneCombo->count() - 1) {
        m_zoneCombo->setCurrentIndex(cur + 1);
    } else if (m_zoneCombo->count() > 0) {
        m_zoneCombo->setCurrentIndex(0);
    }
}

void VisZoneDock::onExternalZoneSelected(int zoneId) {
    m_activeZoneId = zoneId;
    if (zoneId >= 0 && m_chkIsolate && !m_chkIsolate->isChecked()) {
        m_chkIsolate->blockSignals(true);
        m_chkIsolate->setChecked(true);
        m_chkIsolate->blockSignals(false);
    }
    for (int i = 0; i < m_zoneCombo->count(); ++i) {
        if (m_zoneCombo->itemData(i).toInt() == zoneId) {
            m_updatingCombo = true;
            m_zoneCombo->setCurrentIndex(i);
            m_updatingCombo = false;
            updateActiveZoneDetails();
            onIsolationOptionChanged();
            return;
        }
    }

    // If not found in current floor filter, uncheck floor filter and find it
    if (m_chkCurrentFloorOnly->isChecked()) {
        m_chkCurrentFloorOnly->setChecked(false);
        for (int i = 0; i < m_zoneCombo->count(); ++i) {
            if (m_zoneCombo->itemData(i).toInt() == zoneId) {
                m_updatingCombo = true;
                m_zoneCombo->setCurrentIndex(i);
                m_updatingCombo = false;
                updateActiveZoneDetails();
                onIsolationOptionChanged();
                return;
            }
        }
    }
    onIsolationOptionChanged();
}

void VisZoneDock::onIsolationOptionChanged() {
    bool isolate = (m_activeZoneId >= 0) && (m_chkIsolate ? m_chkIsolate->isChecked() : true);
    float dimOpacity = 0.0f;
    if (isolate && m_radioDim->isChecked()) {
        dimOpacity = m_sliderDim->value() / 100.0f;
    }
    emit isolationChanged(isolate, dimOpacity);
}

void VisZoneDock::updateActiveZoneDetails() {
    m_listPortals->clear();
    m_listEntities->clear();

    if (!m_mgr || m_activeZoneId < 0) {
        m_lblStats->setText(QStringLiteral("All zones visible. No single zone isolated."));
        return;
    }

    const VisZone* z = m_mgr->getZone(m_activeZoneId);
    if (!z) {
        m_lblStats->setText(QStringLiteral("Zone not found."));
        return;
    }

    QString floorStr = (z->minFloor == z->maxFloor)
                       ? QString("Floor %1").arg(z->floor)
                       : QString("Floors %1..%2").arg(z->minFloor).arg(z->maxFloor);

    m_lblStats->setText(QString("<b>Zone %1</b> on %2<br>"
                                "Tiles: %3 (%4 m²)<br>"
                                "Grid Bounds: (%5, %6) to (%7, %8)")
                        .arg(z->id + 1)
                        .arg(floorStr)
                        .arg(z->tiles.size())
                        .arg(z->tiles.size() * 9) // approx 3m x 3m tile
                        .arg(z->bounds.left())
                        .arg(z->bounds.top())
                        .arg(z->bounds.right())
                        .arg(z->bounds.bottom()));

    // Populate Portals
    for (int pId : z->portalIndices) {
        const MapPortal* p = m_mgr->getPortal(pId);
        if (!p) continue;
        int otherZone = (p->zoneA == m_activeZoneId) ? p->zoneB : p->zoneA;
        QListWidgetItem* item = new QListWidgetItem(
            QString("Portal #%1 ? Zone %2 (at tile %3, %4)")
                .arg(p->id + 1)
                .arg(otherZone + 1)
                .arg(p->tileA.x())
                .arg(p->tileA.y())
        );
        item->setData(Qt::UserRole, otherZone);
        m_listPortals->addItem(item);
    }

    if (z->portalIndices.empty()) {
        QListWidgetItem* item = new QListWidgetItem(QStringLiteral("(No direct doorway portals)"));
        item->setFlags(Qt::NoItemFlags);
        m_listPortals->addItem(item);
    }

    // Populate Entities
    if (m_map) {
        for (int eIdx : z->entityIndices) {
            if (eIdx >= 0 && eIdx < m_map->placedEntities.size()) {
                const auto& ent = m_map->placedEntities[eIdx];
                QString name = ent.instanceName;
                if (name.isEmpty() && ent.profile) name = ent.profile->name;
                if (name.isEmpty()) name = QString("Entity #%1").arg(eIdx);

                QListWidgetItem* item = new QListWidgetItem(
                    QString("%1 [Floor %2 at (%3, %4)]")
                        .arg(name)
                        .arg(ent.floorLayer)
                        .arg(static_cast<int>(ent.x / 100.0f))
                        .arg(static_cast<int>(std::abs(ent.z) / 100.0f))
                );
                item->setData(Qt::UserRole, eIdx);
                m_listEntities->addItem(item);
            }
        }
    }

    if (z->entityIndices.empty()) {
        QListWidgetItem* item = new QListWidgetItem(QStringLiteral("(No entities inside this zone)"));
        item->setFlags(Qt::NoItemFlags);
        m_listEntities->addItem(item);
    }
}

void VisZoneDock::onPortalDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    QVariant data = item->data(Qt::UserRole);
    if (!data.isValid()) return;
    int targetZone = data.toInt();
    if (targetZone >= 0) {
        onExternalZoneSelected(targetZone);
        emit zoneSelected(targetZone);
    }
}

void VisZoneDock::onEntityClicked(QListWidgetItem* item) {
    if (!item) return;
    QVariant data = item->data(Qt::UserRole);
    if (!data.isValid()) return;
    int entIdx = data.toInt();
    if (entIdx >= 0) {
        emit entitySelected(entIdx);
    }
}

void VisZoneDock::resetToNormalView() {
    m_activeZoneId = -1;
    if (m_zoneCombo && m_zoneCombo->count() > 0) {
        m_updatingCombo = true;
        m_zoneCombo->setCurrentIndex(0); // "All Zones (Normal View)"
        m_updatingCombo = false;
    }
    updateActiveZoneDetails();
    emit zoneSelected(-1);
    emit isolationChanged(false, 0.0f);
}

void VisZoneDock::closeEvent(QCloseEvent* event) {
    resetToNormalView();
    QDockWidget::closeEvent(event);
}

void VisZoneDock::setColorAllZones(bool enabled) {
    if (m_chkColorAll && m_chkColorAll->isChecked() != enabled) {
        m_chkColorAll->blockSignals(true);
        m_chkColorAll->setChecked(enabled);
        m_chkColorAll->blockSignals(false);
    }
}

bool VisZoneDock::isColorAllZones() const {
    return m_chkColorAll ? m_chkColorAll->isChecked() : false;
}

