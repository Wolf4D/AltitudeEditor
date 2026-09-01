#include "EntityInspector.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>

#include <QEvent>

namespace {
class NoWheelFilter : public QObject {
public:
    explicit NoWheelFilter(QObject* parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            event->ignore();
            return true; // Consume wheel event so spinbox doesn't change
        }
        return QObject::eventFilter(obj, event);
    }
};
}

EntityInspector::EntityInspector(QWidget* parent)
    : QDockWidget(QStringLiteral("Entity Properties Inspector"), parent)
{
    setObjectName("EntityInspector");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_noWheelFilter = new NoWheelFilter(this);

    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_headerLabel = new QLabel(QStringLiteral("Select an entity on map or list to edit properties"), container);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setStyleSheet("font-weight: bold; color: #a0c0ff; padding: 4px;");
    layout->addWidget(m_headerLabel);

    m_tree = new QTreeWidget(container);
    m_tree->setHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->resizeSection(0, 140);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree, 1);

    setWidget(container);
}

void EntityInspector::clear() {
    m_currentIndex = -1;
    m_spinX = nullptr;
    m_spinY = nullptr;
    m_spinZ = nullptr;
    m_map.reset();
    m_headerLabel->setText(QStringLiteral("Select an entity on map or list to edit properties"));
    m_tree->clear();
}

void EntityInspector::refreshValues() {
    if (!m_map || m_currentIndex < 0 || m_currentIndex >= m_map->placedEntities.size()) return;
    const PlacedEntity& ent = m_map->placedEntities[m_currentIndex];
    m_isPopulating = true;
    if (m_spinX) m_spinX->setValue(ent.x);
    if (m_spinY) m_spinY->setValue(ent.y);
    if (m_spinZ) m_spinZ->setValue(ent.z);
    m_isPopulating = false;
}

void EntityInspector::addProperty(QTreeWidgetItem* parent, const QString& name, const QString& value, const QString& tip) {
    QTreeWidgetItem* item = new QTreeWidgetItem(parent, {name, value});
    if (!tip.isEmpty()) item->setToolTip(1, tip);
}

void EntityInspector::addWidgetProperty(QTreeWidgetItem* parent, const QString& name, QWidget* widget, const QString& tip) {
    QTreeWidgetItem* item = new QTreeWidgetItem(parent, {name, QString()});
    if (!tip.isEmpty()) item->setToolTip(0, tip);
    if (widget && m_noWheelFilter) {
        widget->installEventFilter(m_noWheelFilter);
        const auto children = widget->findChildren<QWidget*>();
        for (QWidget* child : children) {
            child->installEventFilter(m_noWheelFilter);
        }
    }
    m_tree->setItemWidget(item, 1, widget);
}

void EntityInspector::setEntity(std::shared_ptr<FPSCMap> map, int index) {
    m_isPopulating = true;
    m_tree->clear();
    m_map = map;
    m_currentIndex = index;

    if (!map || index < 0 || index >= map->placedEntities.size()) {
        m_isPopulating = false;
        clear();
        return;
    }

    PlacedEntity& ent = map->placedEntities[index];
    auto prof = ent.profile;

    QString name = ent.instanceName;
    if (name.isEmpty() && prof) name = prof->name;
    m_headerLabel->setText(QString("[%1] %2 (%3)")
        .arg(index)
        .arg(name)
        .arg(prof ? entityCategoryToString(prof->category) : "Unknown"));

    // -------------------------------------------------------------
    // Group 1: General & Identity
    // -------------------------------------------------------------
    QTreeWidgetItem* grpGeneral = new QTreeWidgetItem(m_tree, {QStringLiteral("General & Identity")});
    grpGeneral->setExpanded(true);
    addProperty(grpGeneral, QStringLiteral("Instance Index"), QString::number(index));

    QLineEdit* editName = new QLineEdit(ent.instanceName);
    editName->setPlaceholderText(prof ? prof->name : QStringLiteral("Entity Name"));
    connect(editName, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].instanceName = text;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpGeneral, QStringLiteral("Name"), editName, QStringLiteral("Instance name of entity"));

    addProperty(grpGeneral, QStringLiteral("Category"), prof ? entityCategoryToString(prof->category) : "N/A");

    QComboBox* comboObj = new QComboBox();
    comboObj->addItems({QStringLiteral("0 - None"), QStringLiteral("1 - Primary Objective"), QStringLiteral("2 - Secondary Objective")});
    comboObj->setCurrentIndex(qBound(0, ent.isObjective, 2));
    connect(comboObj, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].isObjective = idx;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpGeneral, QStringLiteral("Objective Goal"), comboObj);

    // -------------------------------------------------------------
    // Group 2: Transform & Placement
    // -------------------------------------------------------------
    QTreeWidgetItem* grpTransform = new QTreeWidgetItem(m_tree, {QStringLiteral("Transform & Placement")});
    grpTransform->setExpanded(true);

    // Pos X
    m_spinX = new QDoubleSpinBox();
    m_spinX->setRange(-50000.0, 50000.0);
    m_spinX->setSingleStep(10.0);
    m_spinX->setDecimals(1);
    m_spinX->setValue(ent.x);
    connect(m_spinX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].x = static_cast<float>(val);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Position X"), m_spinX);

    // Pos Y (Height)
    m_spinY = new QDoubleSpinBox();
    m_spinY->setRange(-10000.0, 50000.0);
    m_spinY->setSingleStep(10.0);
    m_spinY->setDecimals(1);
    m_spinY->setValue(ent.y);
    connect(m_spinY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].y = static_cast<float>(val);
        m_map->placedEntities[m_currentIndex].floorLayer = qBound(0, static_cast<int>(std::floor((val + 25.0) / 100.0)), 20);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Position Y (Height)"), m_spinY);

    // Pos Z (Depth)
    m_spinZ = new QDoubleSpinBox();
    m_spinZ->setRange(-50000.0, 50000.0);
    m_spinZ->setSingleStep(10.0);
    m_spinZ->setDecimals(1);
    m_spinZ->setValue(ent.z);
    connect(m_spinZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].z = static_cast<float>(val);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Position Z (Depth)"), m_spinZ);

    // Rotation Y (Yaw)
    QDoubleSpinBox* spinRotY = new QDoubleSpinBox();
    spinRotY->setRange(-360.0, 360.0);
    spinRotY->setSingleStep(5.0);
    spinRotY->setDecimals(1);
    spinRotY->setValue(ent.ry);
    connect(spinRotY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].ry = static_cast<float>(val);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Rotation Yaw (Y°)"), spinRotY);

    // Rotation X (Pitch)
    QDoubleSpinBox* spinRotX = new QDoubleSpinBox();
    spinRotX->setRange(-360.0, 360.0);
    spinRotX->setSingleStep(5.0);
    spinRotX->setDecimals(1);
    spinRotX->setValue(ent.rx);
    connect(spinRotX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].rx = static_cast<float>(val);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Rotation Pitch (X°)"), spinRotX);

    // Rotation Z (Roll)
    QDoubleSpinBox* spinRotZ = new QDoubleSpinBox();
    spinRotZ->setRange(-360.0, 360.0);
    spinRotZ->setSingleStep(5.0);
    spinRotZ->setDecimals(1);
    spinRotZ->setValue(ent.rz);
    connect(spinRotZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].rz = static_cast<float>(val);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Rotation Roll (Z°)"), spinRotZ);

    // Scale (Inactive in classic FPS Creator engine)
    QDoubleSpinBox* spinScale = new QDoubleSpinBox();
    spinScale->setRange(0.0, 2000.0);
    spinScale->setDecimals(2);
    spinScale->setValue(ent.scale);
    spinScale->setEnabled(false);
    spinScale->setToolTip(QStringLiteral("Scale is defined globally in the entity .FPE file and is not supported per-instance in classic FPS Creator."));
    addWidgetProperty(grpTransform, QStringLiteral("Scale (%) (Inactive)"), spinScale, QStringLiteral("Fixed in .FPE definition; not editable in classic engine"));

    // Static Flag
    QComboBox* comboStatic = new QComboBox();
    comboStatic->addItems({QStringLiteral("0 - Dynamic (Active AI/Physics)"), QStringLiteral("1 - Static (Optimized Pre-baked)")});
    comboStatic->setCurrentIndex(ent.staticFlag != 0 ? 1 : 0);
    connect(comboStatic, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].staticFlag = idx;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpTransform, QStringLiteral("Static Flag"), comboStatic);

    // -------------------------------------------------------------
    // Group 3: Gameplay Stats & Behavior
    // -------------------------------------------------------------
    QTreeWidgetItem* grpStats = new QTreeWidgetItem(m_tree, {QStringLiteral("Gameplay Stats")});
    grpStats->setExpanded(true);

    QSpinBox* spinHealth = new QSpinBox();
    spinHealth->setRange(0, 100000);
    spinHealth->setSingleStep(10);
    spinHealth->setValue(ent.health);
    connect(spinHealth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].health = val;
        m_map->placedEntities[m_currentIndex].strength = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpStats, QStringLiteral("Health / Strength"), spinHealth);

    QSpinBox* spinLives = new QSpinBox();
    spinLives->setRange(0, 100);
    spinLives->setValue(ent.lives);
    connect(spinLives, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].lives = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpStats, QStringLiteral("Lives"), spinLives);

    QSpinBox* spinSpeed = new QSpinBox();
    spinSpeed->setRange(0, 1000);
    spinSpeed->setSingleStep(10);
    spinSpeed->setValue(ent.speed);
    connect(spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].speed = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpStats, QStringLiteral("Speed"), spinSpeed);

    QSpinBox* spinQty = new QSpinBox();
    spinQty->setRange(0, 10000);
    spinQty->setValue(ent.quantity);
    connect(spinQty, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].quantity = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpStats, QStringLiteral("Quantity / Ammo"), spinQty);

    QComboBox* comboImmobile = new QComboBox();
    comboImmobile->addItems({QStringLiteral("0 - Mobile"), QStringLiteral("1 - Immobile")});
    comboImmobile->setCurrentIndex(ent.isImmobile != 0 ? 1 : 0);
    connect(comboImmobile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].isImmobile = idx;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpStats, QStringLiteral("Is Immobile"), comboImmobile);

    // -------------------------------------------------------------
    // Group 4: FPI Scripts & Triggers
    // -------------------------------------------------------------
    QTreeWidgetItem* grpScripts = new QTreeWidgetItem(m_tree, {QStringLiteral("FPI Scripts & Triggers")});
    grpScripts->setExpanded(true);

    auto makeScriptWidget = [this](const QString& scriptVal, auto setter) -> QWidget* {
        QWidget* w = new QWidget();
        QHBoxLayout* hl = new QHBoxLayout(w);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(2);
        QLineEdit* le = new QLineEdit(scriptVal);
        QPushButton* btn = new QPushButton(QStringLiteral("..."));
        btn->setFixedWidth(24);
        hl->addWidget(le, 1);
        hl->addWidget(btn);

        connect(le, &QLineEdit::textChanged, this, [this, setter](const QString& text) {
            if (m_isPopulating || !m_map || m_currentIndex < 0) return;
            setter(text);
            m_map->isModified = true;
            emit entityModified(m_currentIndex);
        });
        connect(btn, &QPushButton::clicked, this, [this, le]() {
            QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select FPI Script"), QStringLiteral("C:/Program Files (x86)/The Game Creators/FPS Creator/Files/scriptbank"), QStringLiteral("FPI Scripts (*.fpi);;All Files (*.*)"));
            if (!path.isEmpty()) {
                QFileInfo fi(path);
                le->setText(fi.fileName());
            }
        });
        return w;
    };

    addWidgetProperty(grpScripts, QStringLiteral("AI Main (Behavior)"), makeScriptWidget(ent.aiMain, [this](const QString& s) {
        m_map->placedEntities[m_currentIndex].aiMain = s;
    }));
    addWidgetProperty(grpScripts, QStringLiteral("AI Init (Spawn)"), makeScriptWidget(ent.aiInit, [this](const QString& s) {
        m_map->placedEntities[m_currentIndex].aiInit = s;
    }));
    addWidgetProperty(grpScripts, QStringLiteral("AI Shoot"), makeScriptWidget(ent.aiShoot, [this](const QString& s) {
        m_map->placedEntities[m_currentIndex].aiShoot = s;
    }));
    addWidgetProperty(grpScripts, QStringLiteral("AI Destroy (Death)"), makeScriptWidget(ent.aiDestroy, [this](const QString& s) {
        m_map->placedEntities[m_currentIndex].aiDestroy = s;
    }));

    QLineEdit* editKey = new QLineEdit(ent.useKey);
    connect(editKey, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].useKey = text;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpScripts, QStringLiteral("Required Key"), editKey);

    QLineEdit* editIfUsed = new QLineEdit(ent.ifUsed);
    connect(editIfUsed, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].ifUsed = text;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpScripts, QStringLiteral("If Used Trigger"), editIfUsed);

    // -------------------------------------------------------------
    // Group 5: Lighting & Zones
    // -------------------------------------------------------------
    QTreeWidgetItem* grpSpecial = new QTreeWidgetItem(m_tree, {QStringLiteral("Lighting & Zones")});
    grpSpecial->setExpanded(true);

    QDoubleSpinBox* spinLRange = new QDoubleSpinBox();
    spinLRange->setRange(0.0, 5000.0);
    spinLRange->setSingleStep(10.0);
    spinLRange->setValue(ent.lightRange);
    connect(spinLRange, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].lightRange = static_cast<float>(val);
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpSpecial, QStringLiteral("Light Range"), spinLRange);

    QPushButton* btnCol = new QPushButton();
    btnCol->setFixedHeight(22);
    btnCol->setCursor(Qt::PointingHandCursor);
    auto updateColStyle = [btnCol](const QColor& c) {
        QColor col = (c.isValid() && (c.red() > 0 || c.green() > 0 || c.blue() > 0)) ? c : QColor(255, 255, 255);
        btnCol->setText(QString("RGB(%1, %2, %3)").arg(col.red()).arg(col.green()).arg(col.blue()));
        btnCol->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; font-weight: bold; border: 1px solid #555; border-radius: 3px; padding: 2px 6px; } "
                                      "QPushButton:hover { border: 1px solid #fff; }")
            .arg(col.name())
            .arg(col.lightness() > 128 ? "#111111" : "#ffffff"));
    };
    updateColStyle(ent.lightColor);

    connect(btnCol, &QPushButton::clicked, this, [this, updateColStyle, spinLRange]() {
        if (!m_map || m_currentIndex < 0) return;
        QColor cur = m_map->placedEntities[m_currentIndex].lightColor;
        if (!cur.isValid() || (cur.red() == 0 && cur.green() == 0 && cur.blue() == 0)) {
            cur = QColor(255, 255, 255);
        }
        QColor chosen = QColorDialog::getColor(cur, this, QStringLiteral("Select Light Color"));
        if (chosen.isValid()) {
            m_map->placedEntities[m_currentIndex].lightColor = chosen;
            if (m_map->placedEntities[m_currentIndex].lightRange <= 0) {
                float defRange = (m_map->placedEntities[m_currentIndex].profile && m_map->placedEntities[m_currentIndex].profile->lightRange > 0)
                    ? m_map->placedEntities[m_currentIndex].profile->lightRange : 300.0f;
                m_map->placedEntities[m_currentIndex].lightRange = defRange;
                spinLRange->setValue(defRange);
            }
            updateColStyle(chosen);
            m_map->isModified = true;
            emit entityModified(m_currentIndex);
        }
    });
    addWidgetProperty(grpSpecial, QStringLiteral("Light Color"), btnCol);

    // Zone Bounding Volume
    QSpinBox* spinZ1 = new QSpinBox(); spinZ1->setRange(-50000, 50000); spinZ1->setValue(ent.trigX1);
    connect(spinZ1, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].trigX1 = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpSpecial, QStringLiteral("Zone Area X1"), spinZ1);

    QSpinBox* spinZ2 = new QSpinBox(); spinZ2->setRange(-50000, 50000); spinZ2->setValue(ent.trigX2);
    connect(spinZ2, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].trigX2 = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpSpecial, QStringLiteral("Zone Area X2"), spinZ2);

    // -------------------------------------------------------------
    // Group 6: Physics & Collision
    // -------------------------------------------------------------
    QTreeWidgetItem* grpPhysics = new QTreeWidgetItem(m_tree, {QStringLiteral("Physics & Collision")});
    grpPhysics->setExpanded(false);

    QComboBox* comboPhys = new QComboBox();
    comboPhys->addItems({
        QStringLiteral("0 - None (Standard / Scripted)"),
        QStringLiteral("1 - Dynamic (ODE/Newton Rigid Body)"),
        QStringLiteral("2 - Immobile Polylist Collision"),
        QStringLiteral("3 - Dynamic (Pushable)"),
        QStringLiteral("4 - Character Capsule Controller")
    });
    comboPhys->setCurrentIndex(qBound(0, ent.physics, 4));
    connect(comboPhys, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].physics = idx;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpPhysics, QStringLiteral("Physics Mode"), comboPhys);

    QSpinBox* spinWeight = new QSpinBox();
    spinWeight->setRange(0, 100000);
    spinWeight->setValue(ent.phyWeight);
    connect(spinWeight, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].phyWeight = val;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpPhysics, QStringLiteral("Physics Weight"), spinWeight);

    QComboBox* comboExpl = new QComboBox();
    comboExpl->addItems({QStringLiteral("0 - No"), QStringLiteral("1 - Explodable (Destructible)")});
    comboExpl->setCurrentIndex(ent.explodable != 0 ? 1 : 0);
    connect(comboExpl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isPopulating || !m_map || m_currentIndex < 0) return;
        m_map->placedEntities[m_currentIndex].explodable = idx;
        m_map->isModified = true;
        emit entityModified(m_currentIndex);
    });
    addWidgetProperty(grpPhysics, QStringLiteral("Explodable"), comboExpl);

    // -------------------------------------------------------------
    // Group 7: Assets & Resource Info (Read-only)
    // -------------------------------------------------------------
    if (prof) {
        QTreeWidgetItem* grpAssets = new QTreeWidgetItem(m_tree, {QStringLiteral("Assets & Memory Footprint")});
        grpAssets->setExpanded(false);
        addProperty(grpAssets, QStringLiteral("FPE Profile"), prof->relPath);
        if (!prof->modelPath.isEmpty()) {
            addProperty(grpAssets, QStringLiteral("3D Mesh (.X)"), QString("%1 (%2 KB)")
                .arg(prof->modelPath)
                .arg(prof->meshSizeBytes / 1024.0, 0, 'f', 1));
        }
        if (!prof->texturePath.isEmpty()) {
            addProperty(grpAssets, QStringLiteral("Diffuse Texture"), QString("%1 [%2x%3] (%4 MB in RAM)")
                .arg(prof->texturePath)
                .arg(prof->texWidth).arg(prof->texHeight)
                .arg(prof->diffuseSizeBytes / (1024.0 * 1024.0), 0, 'f', 2));
        }
        if (prof->audioSizeBytes > 0) {
            addProperty(grpAssets, QStringLiteral("Audio Buffers"), QString("%1 KB").arg(prof->audioSizeBytes / 1024.0, 0, 'f', 1));
        }
        addProperty(grpAssets, QStringLiteral("Est. RAM / Type"), QString("%1 MB").arg(prof->estimatedRAMBytes / (1024.0 * 1024.0), 0, 'f', 2));
    }

    m_isPopulating = false;
}
