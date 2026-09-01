#include "EntitySearchDock.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>

EntitySearchDock::EntitySearchDock(QWidget* parent)
    : QDockWidget(QStringLiteral("Entity Search & Palette"), parent)
{
    setObjectName("EntitySearchDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Search input
    m_searchEdit = new QLineEdit(container);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search entity name or script..."));
    m_searchEdit->setClearButtonEnabled(true);
    layout->addWidget(m_searchEdit);

    // Category filter combo
    m_categoryCombo = new QComboBox(container);
    m_categoryCombo->addItem(QStringLiteral("All Categories"), -1);
    m_categoryCombo->addItem(QStringLiteral("Player Start"), static_cast<int>(EntityCategory::PlayerStart));
    m_categoryCombo->addItem(QStringLiteral("Characters / AI"), static_cast<int>(EntityCategory::Character));
    m_categoryCombo->addItem(QStringLiteral("Weapons"), static_cast<int>(EntityCategory::Weapon));
    m_categoryCombo->addItem(QStringLiteral("Ammo"), static_cast<int>(EntityCategory::Ammo));
    m_categoryCombo->addItem(QStringLiteral("Light Sources"), static_cast<int>(EntityCategory::Light));
    m_categoryCombo->addItem(QStringLiteral("Trigger Zones"), static_cast<int>(EntityCategory::Zone));
    m_categoryCombo->addItem(QStringLiteral("Doors & Obstacles"), static_cast<int>(EntityCategory::Door));
    m_categoryCombo->addItem(QStringLiteral("Items & Pickups"), static_cast<int>(EntityCategory::Item));
    m_categoryCombo->addItem(QStringLiteral("Scenery & Props"), static_cast<int>(EntityCategory::Scenery));
    layout->addWidget(m_categoryCombo);

    // Current floor toggle
    m_currentFloorOnlyCheck = new QCheckBox(QStringLiteral("Show current floor only"), container);
    m_currentFloorOnlyCheck->setChecked(false);
    layout->addWidget(m_currentFloorOnlyCheck);

    // Entity table
    m_table = new QTableWidget(container);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Icon"), QStringLiteral("Name"), QStringLiteral("Category"), QStringLiteral("Floor")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setIconSize(QSize(28, 28));
    layout->addWidget(m_table, 1);

    // Count label
    m_countLabel = new QLabel(QStringLiteral("0 entities found"), container);
    m_countLabel->setStyleSheet("color: #888899; font-size: 11px;");
    layout->addWidget(m_countLabel);

    setWidget(container);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &EntitySearchDock::onFilterChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EntitySearchDock::onFilterChanged);
    connect(m_currentFloorOnlyCheck, &QCheckBox::toggled, this, &EntitySearchDock::onFilterChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &EntitySearchDock::onItemDoubleClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &EntitySearchDock::onItemSelectionChanged);
}

void EntitySearchDock::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    rebuildTable();
}

void EntitySearchDock::setCurrentFloor(int floor) {
    m_currentFloor = floor;
    if (m_currentFloorOnlyCheck->isChecked()) {
        rebuildTable();
    }
}

void EntitySearchDock::selectEntity(int index) {
    if (m_isUpdatingSelection || !m_map) return;

    m_isUpdatingSelection = true;
    if (index < 0) {
        m_table->clearSelection();
        m_isUpdatingSelection = false;
        return;
    }

    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* item = m_table->item(r, 0);
        if (item && item->data(Qt::UserRole).toInt() == index) {
            m_table->selectRow(r);
            m_table->scrollToItem(item);
            break;
        }
    }
    m_isUpdatingSelection = false;
}

void EntitySearchDock::onFilterChanged() {
    rebuildTable();
}

void EntitySearchDock::onItemDoubleClicked(QTableWidgetItem* item) {
    if (!item) return;
    int row = item->row();
    QTableWidgetItem* idItem = m_table->item(row, 0);
    if (idItem) {
        int entIdx = idItem->data(Qt::UserRole).toInt();
        emit focusEntityRequested(entIdx);
    }
}

void EntitySearchDock::onItemSelectionChanged() {
    if (m_isUpdatingSelection) return;
    auto selectedItems = m_table->selectedItems();
    if (selectedItems.isEmpty()) return;
    int row = selectedItems.first()->row();
    QTableWidgetItem* idItem = m_table->item(row, 0);
    if (idItem) {
        int entIdx = idItem->data(Qt::UserRole).toInt();
        emit entitySelected(entIdx);
    }
}

void EntitySearchDock::rebuildTable() {
    m_table->setRowCount(0);
    if (!m_map) {
        m_countLabel->setText(QStringLiteral("0 entities found"));
        return;
    }

    QString search = m_searchEdit->text().trimmed().toLower();
    int selectedCat = m_categoryCombo->currentData().toInt();
    bool currentFloorOnly = m_currentFloorOnlyCheck->isChecked();

    int matchedCount = 0;

    for (int i = 0; i < m_map->placedEntities.size(); ++i) {
        const PlacedEntity& ent = m_map->placedEntities[i];

        if (currentFloorOnly && ent.floorLayer != m_currentFloor) {
            continue;
        }

        EntityCategory cat = ent.profile ? ent.profile->category : EntityCategory::Unknown;
        if (selectedCat >= 0 && static_cast<int>(cat) != selectedCat) {
            continue;
        }

        QString name = ent.instanceName;
        if (name.isEmpty() && ent.profile) name = ent.profile->name;
        QString script = ent.aiMain;

        if (!search.isEmpty()) {
            bool matches = name.toLower().contains(search) ||
                            script.toLower().contains(search) ||
                            (ent.profile && ent.profile->relPath.toLower().contains(search));
            if (!matches) continue;
        }

        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Column 0: Icon
        QPixmap iconPx;
        if (ent.profile && !ent.profile->iconBmpPath.isEmpty()) {
            iconPx = AssetManager::instance().loadIcon(ent.profile->iconBmpPath);
        }
        if (iconPx.isNull()) {
            // Generate colored badge icon
            QPixmap gen(28, 28);
            gen.fill(Qt::transparent);
            QPainter gp(&gen);
            gp.setRenderHint(QPainter::Antialiasing);
            gp.setBrush(entityCategoryColor(cat));
            gp.setPen(Qt::NoPen);
            gp.drawEllipse(2, 2, 24, 24);
            gp.setPen(Qt::white);
            gp.setFont(QFont("Segoe UI", 9, QFont::Bold));
            gp.drawText(gen.rect(), Qt::AlignCenter, name.left(1).toUpper());
            iconPx = gen;
        }

        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(QIcon(iconPx));
        iconItem->setData(Qt::UserRole, i);
        m_table->setItem(row, 0, iconItem);

        // Column 1: Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setToolTip(QString("Instance #%1\nProfile: %2\nScript: %3")
            .arg(i)
            .arg(ent.profile ? ent.profile->relPath : "N/A")
            .arg(script.isEmpty() ? "None" : script));
        m_table->setItem(row, 1, nameItem);

        // Column 2: Category
        QTableWidgetItem* catItem = new QTableWidgetItem(entityCategoryToString(cat));
        catItem->setForeground(entityCategoryColor(cat).lighter(120));
        m_table->setItem(row, 2, catItem);

        // Column 3: Floor
        QTableWidgetItem* floorItem = new QTableWidgetItem(QString::number(ent.floorLayer));
        floorItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, floorItem);

        matchedCount++;
    }

    m_countLabel->setText(QString("%1 entities found (out of %2 total)")
        .arg(matchedCount)
        .arg(m_map->placedEntities.size()));
}
