#ifndef ENTITYSEARCHDOCK_H
#define ENTITYSEARCHDOCK_H

#include "FPSCData.h"
#include <QDockWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QLabel>
#include <memory>

class EntitySearchDock : public QDockWidget {
    Q_OBJECT
public:
    explicit EntitySearchDock(QWidget* parent = nullptr);
    ~EntitySearchDock() override = default;

    void setMap(std::shared_ptr<FPSCMap> map);
    void setCurrentFloor(int floor);
    void selectEntity(int index);

signals:
    void entitySelected(int index);
    void focusEntityRequested(int index);

private slots:
    void onFilterChanged();
    void onItemDoubleClicked(QTableWidgetItem* item);
    void onItemSelectionChanged();

private:
    void rebuildTable();

    std::shared_ptr<FPSCMap> m_map;
    int m_currentFloor = 0;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QCheckBox* m_currentFloorOnlyCheck = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_countLabel = nullptr;

    bool m_isUpdatingSelection = false;
};

#endif // ENTITYSEARCHDOCK_H
