#include "EntitySearchDock.h"
#include "AssetManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QMenu>
#include <QKeyEvent>

namespace {
class NumericTableWidgetItem : public QTableWidgetItem {
public:
    explicit NumericTableWidgetItem(int val) : QTableWidgetItem(QString::number(val)), m_val(val) {}
    bool operator<(const QTableWidgetItem& other) const override {
        const auto* numOther = dynamic_cast<const NumericTableWidgetItem*>(&other);
        if (numOther) {
            return m_val < numOther->m_val;
        }
        return QTableWidgetItem::operator<(other);
    }
private:
    int m_val = 0;
};
}

EntitySearchDock::EntitySearchDock(QWidget* parent)
    : QDockWidget(tr("Entity Search & Palette"), parent)
{
    setObjectName("EntitySearchDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Search bar with explicit Search button
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(4);

    m_searchEdit = new QLineEdit(container);
    m_searchEdit->setPlaceholderText(tr("Search entity name or script..."));
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit, 1);

    m_searchBtn = new QPushButton(tr("🔍 Search"), container);
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setStyleSheet(QStringLiteral("QPushButton { padding: 3px 8px; font-weight: bold; }"));
    searchLayout->addWidget(m_searchBtn);
    layout->addLayout(searchLayout);

    // Category filter combo
    m_categoryCombo = new QComboBox(container);
    m_categoryCombo->addItem(tr("All Categories"), -1);
    m_categoryCombo->addItem(tr("Player Start"), static_cast<int>(EntityCategory::PlayerStart));
    m_categoryCombo->addItem(tr("Characters / AI"), static_cast<int>(EntityCategory::Character));
    m_categoryCombo->addItem(tr("Weapons"), static_cast<int>(EntityCategory::Weapon));
    m_categoryCombo->addItem(tr("Ammo"), static_cast<int>(EntityCategory::Ammo));
    m_categoryCombo->addItem(tr("Light Sources"), static_cast<int>(EntityCategory::Light));
    m_categoryCombo->addItem(tr("Trigger Zones"), static_cast<int>(EntityCategory::Zone));
    m_categoryCombo->addItem(tr("Doors & Obstacles"), static_cast<int>(EntityCategory::Door));
    m_categoryCombo->addItem(tr("Items & Pickups"), static_cast<int>(EntityCategory::Item));
    m_categoryCombo->addItem(tr("Scenery & Props"), static_cast<int>(EntityCategory::Scenery));
    layout->addWidget(m_categoryCombo);

    // Trait / Property Filter combo
    m_traitCombo = new QComboBox(container);
    m_traitCombo->addItem(tr("All Types & Properties"), 0);
    m_traitCombo->addItem(tr("Characters / AI (Enemies & NPCs)"), 1);
    m_traitCombo->addItem(tr("Light Sources (Color/Range)"), 2);
    m_traitCombo->addItem(tr("Trigger Zones (Areas)"), 3);
    m_traitCombo->addItem(tr("Dynamic Physics (ODE)"), 4);
    m_traitCombo->addItem(tr("Static (Pre-baked)"), 5);
    m_traitCombo->addItem(tr("With Custom AI Scripts"), 6);
    layout->addWidget(m_traitCombo);

    // Current floor toggle
    m_currentFloorOnlyCheck = new QCheckBox(tr("Show current floor only"), container);
    m_currentFloorOnlyCheck->setChecked(false);
    layout->addWidget(m_currentFloorOnlyCheck);

    // Entity table
    m_table = new QTableWidget(container);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Icon"), tr("Name"), tr("Category"), tr("Floor")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setIconSize(QSize(28, 28));
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setSortingEnabled(true);
    m_table->installEventFilter(this);
    layout->addWidget(m_table, 1);

    // Bottom action row: count label and Delete button
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_countLabel = new QLabel(tr("0 entities found"), container);
    m_countLabel->setStyleSheet("color: #888899; font-size: 11px;");
    bottomLayout->addWidget(m_countLabel, 1);

    m_deleteBtn = new QPushButton(tr("🗑️ Delete"), container);
    m_deleteBtn->setEnabled(false);
    m_deleteBtn->setToolTip(tr("Delete the selected entity from map (Del)"));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #382428; color: #ff8899; border: 1px solid #773344; padding: 3px 10px; border-radius: 3px; font-weight: bold; } "
        "QPushButton:hover { background-color: #552830; color: #ffb0c0; border: 1px solid #aa4455; } "
        "QPushButton:disabled { background-color: #22252a; color: #555566; border: 1px solid #333640; }"
    ));
    bottomLayout->addWidget(m_deleteBtn);
    layout->addLayout(bottomLayout);

    setWidget(container);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &EntitySearchDock::onFilterChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &EntitySearchDock::onFilterChanged);
    connect(m_searchBtn, &QPushButton::clicked, this, &EntitySearchDock::onFilterChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EntitySearchDock::onFilterChanged);
    connect(m_traitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EntitySearchDock::onFilterChanged);
    connect(m_currentFloorOnlyCheck, &QCheckBox::toggled, this, &EntitySearchDock::onFilterChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &EntitySearchDock::onItemDoubleClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &EntitySearchDock::onItemSelectionChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &EntitySearchDock::showTableContextMenu);
    connect(m_deleteBtn, &QPushButton::clicked, this, &EntitySearchDock::onDeleteClicked);
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

int EntitySearchDock::selectedEntityIndex() const {
    auto selectedItems = m_table->selectedItems();
    if (selectedItems.isEmpty()) return -1;
    int row = selectedItems.first()->row();
    QTableWidgetItem* idItem = m_table->item(row, 0);
    return idItem ? idItem->data(Qt::UserRole).toInt() : -1;
}

void EntitySearchDock::selectEntity(int index) {
    if (m_isUpdatingSelection || !m_map) return;

    m_isUpdatingSelection = true;
    if (index < 0) {
        m_table->clearSelection();
        m_deleteBtn->setEnabled(false);
        m_isUpdatingSelection = false;
        return;
    }

    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* item = m_table->item(r, 0);
        if (item && item->data(Qt::UserRole).toInt() == index) {
            m_table->selectRow(r);
            m_table->scrollToItem(item);
            m_deleteBtn->setEnabled(true);
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
    m_deleteBtn->setEnabled(!selectedItems.isEmpty());
    if (selectedItems.isEmpty()) return;
    int row = selectedItems.first()->row();
    QTableWidgetItem* idItem = m_table->item(row, 0);
    if (idItem) {
        int entIdx = idItem->data(Qt::UserRole).toInt();
        emit entitySelected(entIdx);
    }
}

void EntitySearchDock::onDeleteClicked() {
    int entIdx = selectedEntityIndex();
    if (entIdx >= 0) {
        emit entityDeleteRequested(entIdx);
    }
}

void EntitySearchDock::showTableContextMenu(const QPoint& pos) {
    QTableWidgetItem* item = m_table->itemAt(pos);
    if (!item) return;
    int row = item->row();
    QTableWidgetItem* idItem = m_table->item(row, 0);
    if (!idItem) return;
    int entIdx = idItem->data(Qt::UserRole).toInt();

    QMenu menu(this);
    QAction* actInspect = menu.addAction(tr("Inspect Properties"));
    QAction* actFocus = menu.addAction(tr("Focus on Canvas (Double-Click)"));
    menu.addSeparator();
    QAction* actDelete = menu.addAction(tr("🗑️ Delete Entity (Del)"));

    QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == actInspect) {
        emit entitySelected(entIdx);
    } else if (chosen == actFocus) {
        emit focusEntityRequested(entIdx);
    } else if (chosen == actDelete) {
        emit entityDeleteRequested(entIdx);
    }
}

bool EntitySearchDock::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
            onDeleteClicked();
            return true;
        }
    }
    return QDockWidget::eventFilter(watched, event);
}

void EntitySearchDock::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDockWidget::changeEvent(event);
}

void EntitySearchDock::retranslateUi() {
    setWindowTitle(tr("Entity Search & Palette"));
    if (m_searchEdit) m_searchEdit->setPlaceholderText(tr("Search entity name or script..."));
    if (m_searchBtn) m_searchBtn->setText(tr("🔍 Search"));

    if (m_categoryCombo) {
        int curCat = m_categoryCombo->currentData().toInt();
        m_categoryCombo->blockSignals(true);
        m_categoryCombo->clear();
        m_categoryCombo->addItem(tr("All Categories"), -1);
        m_categoryCombo->addItem(tr("Player Start"), static_cast<int>(EntityCategory::PlayerStart));
        m_categoryCombo->addItem(tr("Characters / AI"), static_cast<int>(EntityCategory::Character));
        m_categoryCombo->addItem(tr("Weapons"), static_cast<int>(EntityCategory::Weapon));
        m_categoryCombo->addItem(tr("Ammo"), static_cast<int>(EntityCategory::Ammo));
        m_categoryCombo->addItem(tr("Light Sources"), static_cast<int>(EntityCategory::Light));
        m_categoryCombo->addItem(tr("Trigger Zones"), static_cast<int>(EntityCategory::Zone));
        m_categoryCombo->addItem(tr("Doors & Obstacles"), static_cast<int>(EntityCategory::Door));
        m_categoryCombo->addItem(tr("Items & Pickups"), static_cast<int>(EntityCategory::Item));
        m_categoryCombo->addItem(tr("Scenery & Props"), static_cast<int>(EntityCategory::Scenery));
        for (int i = 0; i < m_categoryCombo->count(); ++i) {
            if (m_categoryCombo->itemData(i).toInt() == curCat) {
                m_categoryCombo->setCurrentIndex(i);
                break;
            }
        }
        m_categoryCombo->blockSignals(false);
    }

    if (m_traitCombo) {
        int curTrait = m_traitCombo->currentIndex();
        m_traitCombo->blockSignals(true);
        m_traitCombo->clear();
        m_traitCombo->addItem(tr("All Types & Properties"), 0);
        m_traitCombo->addItem(tr("Characters / AI (Enemies & NPCs)"), 1);
        m_traitCombo->addItem(tr("Light Sources (Color/Range)"), 2);
        m_traitCombo->addItem(tr("Trigger Zones (Areas)"), 3);
        m_traitCombo->addItem(tr("Dynamic Physics (ODE)"), 4);
        m_traitCombo->addItem(tr("Static (Pre-baked)"), 5);
        m_traitCombo->addItem(tr("With Custom AI Scripts"), 6);
        m_traitCombo->setCurrentIndex(curTrait >= 0 ? curTrait : 0);
        m_traitCombo->blockSignals(false);
    }

    if (m_currentFloorOnlyCheck) m_currentFloorOnlyCheck->setText(tr("Show current floor only"));

    if (m_table) {
        m_table->setHorizontalHeaderLabels({tr("Icon"), tr("Name"), tr("Category"), tr("Floor")});
    }

    if (m_deleteBtn) {
        m_deleteBtn->setText(tr("🗑️ Delete"));
        m_deleteBtn->setToolTip(tr("Delete the selected entity from map (Del)"));
    }

    rebuildTable();
}

void EntitySearchDock::rebuildTable() {
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    if (!m_map) {
        m_countLabel->setText(tr("0 entities found"));
        m_deleteBtn->setEnabled(false);
        return;
    }

    QString search = m_searchEdit->text().trimmed().toLower();
    int selectedCat = m_categoryCombo->currentData().toInt();
    int traitFilter = m_traitCombo->currentIndex();
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

        // Trait filter
        if (traitFilter == 1) { // Characters / AI
            if (cat != EntityCategory::Character && (!ent.profile || !ent.profile->isCharacter)) {
                continue;
            }
        } else if (traitFilter == 2) { // Lights
            if (cat != EntityCategory::Light && ent.lightRange <= 0 && (!ent.profile || ent.profile->lightRange <= 0))
                continue;
        } else if (traitFilter == 3) { // Trigger Zones
            if (cat != EntityCategory::Zone && ent.trigX1 == 0 && ent.trigX2 == 0)
                continue;
        } else if (traitFilter == 4) { // Dynamic Physics
            if (ent.physics != 1)
                continue;
        } else if (traitFilter == 5) { // Static
            if (ent.staticFlag != 1 && ent.physics != 0)
                continue;
        } else if (traitFilter == 6) { // Custom Scripts
            if (ent.aiMain.isEmpty() && ent.aiInit.isEmpty() && ent.aiShoot.isEmpty())
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
        } else if (cat == EntityCategory::Light || ent.lightRange > 0) {
            // Overlay colored light badge on table thumbnail
            QPixmap tinted = iconPx;
            QPainter ip(&tinted);
            ip.setRenderHint(QPainter::Antialiasing);
            ip.setBrush(ent.lightColor);
            ip.setPen(QPen(Qt::black, 1.5f));
            ip.drawEllipse(tinted.width() - 14, tinted.height() - 14, 12, 12);
            iconPx = tinted;
        }

        QTableWidgetItem* iconItem = new NumericTableWidgetItem(i);
        iconItem->setIcon(QIcon(iconPx));
        iconItem->setData(Qt::UserRole, i);
        iconItem->setText(QString::number(i));
        iconItem->setForeground(QColor(120, 130, 150));
        m_table->setItem(row, 0, iconItem);

        // Column 1: Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setToolTip(tr("Instance #%1\nProfile: %2\nScript: %3")
            .arg(i)
            .arg(ent.profile ? ent.profile->relPath : tr("N/A"))
            .arg(script.isEmpty() ? tr("None") : script));
        m_table->setItem(row, 1, nameItem);

        // Column 2: Category
        QTableWidgetItem* catItem = new QTableWidgetItem(entityCategoryToString(cat));
        catItem->setForeground(entityCategoryColor(cat).lighter(120));
        m_table->setItem(row, 2, catItem);

        // Column 3: Floor
        QTableWidgetItem* floorItem = new NumericTableWidgetItem(ent.floorLayer);
        floorItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, floorItem);

        matchedCount++;
    }

    m_table->setSortingEnabled(true);

    m_countLabel->setText(tr("%1 entities found (out of %2 total)")
        .arg(matchedCount)
        .arg(m_map->placedEntities.size()));
}
