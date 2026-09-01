#include "EntityInspector.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHeaderView>

EntityInspector::EntityInspector(QWidget* parent)
    : QDockWidget(QStringLiteral("Entity Properties Inspector"), parent)
{
    setObjectName("EntityInspector");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_headerLabel = new QLabel(QStringLiteral("Select an entity on map or list to view properties"), container);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setStyleSheet("font-weight: bold; color: #a0c0ff; padding: 4px;");
    layout->addWidget(m_headerLabel);

    m_tree = new QTreeWidget(container);
    m_tree->setHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree, 1);

    setWidget(container);
}

void EntityInspector::clear() {
    m_headerLabel->setText(QStringLiteral("Select an entity on map or list to view properties"));
    m_tree->clear();
}

void EntityInspector::addProperty(QTreeWidgetItem* parent, const QString& name, const QString& value, const QString& tip) {
    QTreeWidgetItem* item = new QTreeWidgetItem(parent, {name, value});
    if (!tip.isEmpty()) item->setToolTip(1, tip);
}

void EntityInspector::setEntity(std::shared_ptr<FPSCMap> map, int index) {
    m_tree->clear();
    if (!map || index < 0 || index >= map->placedEntities.size()) {
        clear();
        return;
    }

    const PlacedEntity& ent = map->placedEntities[index];
    auto prof = ent.profile;

    QString name = ent.instanceName;
    if (name.isEmpty() && prof) name = prof->name;
    m_headerLabel->setText(QString("[%1] %2 (%3)")
        .arg(index)
        .arg(name)
        .arg(prof ? entityCategoryToString(prof->category) : "Unknown"));

    // Group 1: General & Coordinates
    QTreeWidgetItem* grpGeneral = new QTreeWidgetItem(m_tree, {QStringLiteral("General & Transform")});
    grpGeneral->setExpanded(true);
    addProperty(grpGeneral, QStringLiteral("Instance Index"), QString::number(index));
    addProperty(grpGeneral, QStringLiteral("Name"), name);
    addProperty(grpGeneral, QStringLiteral("Category"), prof ? entityCategoryToString(prof->category) : "N/A");
    addProperty(grpGeneral, QStringLiteral("Floor Layer"), QString("%1 (Y: %2)").arg(ent.floorLayer).arg(ent.y, 0, 'f', 1));
    addProperty(grpGeneral, QStringLiteral("Position (X, Y, Z)"), QString("(%1, %2, %3)").arg(ent.x, 0, 'f', 1).arg(ent.y, 0, 'f', 1).arg(ent.z, 0, 'f', 1));
    addProperty(grpGeneral, QStringLiteral("Rotation (X, Y, Z)"), QString("(%1°, %2°, %3°)").arg(ent.rx, 0, 'f', 1).arg(ent.ry, 0, 'f', 1).arg(ent.rz, 0, 'f', 1));
    addProperty(grpGeneral, QStringLiteral("Scale"), QString("%1%").arg(ent.scale));
    addProperty(grpGeneral, QStringLiteral("Static Flag"), QString::number(ent.staticFlag));

    // Group 2: Gameplay Stats
    QTreeWidgetItem* grpStats = new QTreeWidgetItem(m_tree, {QStringLiteral("Gameplay Stats")});
    grpStats->setExpanded(true);
    addProperty(grpStats, QStringLiteral("Health / HP"), QString::number(ent.health));
    addProperty(grpStats, QStringLiteral("Lives"), QString::number(ent.lives));
    addProperty(grpStats, QStringLiteral("Speed"), QString::number(ent.speed));
    if (prof && !prof->gunName.isEmpty()) addProperty(grpStats, QStringLiteral("Equipped Gun"), prof->gunName);
    if (prof && prof->ammoQty > 0) addProperty(grpStats, QStringLiteral("Ammo Quantity"), QString::number(prof->ammoQty));
    if (prof && prof->isImmune) addProperty(grpStats, QStringLiteral("Damage Immune"), QStringLiteral("Yes"));

    // Group 3: FPI Behavior Scripts
    QTreeWidgetItem* grpScripts = new QTreeWidgetItem(m_tree, {QStringLiteral("FPI Scripts & Logic")});
    grpScripts->setExpanded(true);
    addProperty(grpScripts, QStringLiteral("AI Init"), ent.aiInit.isEmpty() ? (prof ? prof->aiInit : "none") : ent.aiInit);
    addProperty(grpScripts, QStringLiteral("AI Main"), ent.aiMain.isEmpty() ? (prof ? prof->aiMain : "none") : ent.aiMain);
    addProperty(grpScripts, QStringLiteral("AI Shoot"), ent.aiShoot.isEmpty() ? (prof ? prof->aiShoot : "none") : ent.aiShoot);
    addProperty(grpScripts, QStringLiteral("AI Destroy"), ent.aiDestroy.isEmpty() ? (prof ? prof->aiDestroy : "none") : ent.aiDestroy);
    if (!ent.useKey.isEmpty()) addProperty(grpScripts, QStringLiteral("Required Key"), ent.useKey);
    if (!ent.ifUsed.isEmpty()) addProperty(grpScripts, QStringLiteral("If Used Trigger"), ent.ifUsed);

    // Group 4: Lighting & Triggers
    if (ent.lightRange > 0 || (ent.trigX1 != ent.trigX2 || ent.trigZ1 != ent.trigZ2)) {
        QTreeWidgetItem* grpSpecial = new QTreeWidgetItem(m_tree, {QStringLiteral("Lighting & Zones")});
        grpSpecial->setExpanded(true);
        if (ent.lightRange > 0) {
            addProperty(grpSpecial, QStringLiteral("Light Range"), QString::number(ent.lightRange));
            addProperty(grpSpecial, QStringLiteral("Light Color RGB"), QString("(%1, %2, %3)")
                .arg(ent.lightColor.red()).arg(ent.lightColor.green()).arg(ent.lightColor.blue()));
        }
        if (ent.trigX1 != ent.trigX2 || ent.trigZ1 != ent.trigZ2) {
            addProperty(grpSpecial, QStringLiteral("Zone Box (X1..X2)"), QString("%1 .. %2").arg(ent.trigX1).arg(ent.trigX2));
            addProperty(grpSpecial, QStringLiteral("Zone Box (Z1..Z2)"), QString("%1 .. %2").arg(ent.trigZ1).arg(ent.trigZ2));
        }
    }

    // Group 5: Asset Files & Memory Footprint
    if (prof) {
        QTreeWidgetItem* grpAssets = new QTreeWidgetItem(m_tree, {QStringLiteral("Assets & Memory Footprint")});
        grpAssets->setExpanded(true);
        addProperty(grpAssets, QStringLiteral("FPE File"), prof->relPath);
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
}
