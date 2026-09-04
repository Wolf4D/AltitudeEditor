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

#include <QPushButton>

class EntitySearchDock : public QDockWidget {
    Q_OBJECT
public:
    explicit EntitySearchDock(QWidget* parent = nullptr);
    ~EntitySearchDock() override = default;

    void setMap(std::shared_ptr<FPSCMap> map);
    void setCurrentFloor(int floor);
    void selectEntity(int index);
    int selectedEntityIndex() const;
    void rebuildTable();

signals:
    void entitySelected(int index);
    void focusEntityRequested(int index);
    void entityDeleteRequested(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onFilterChanged();
    void onItemDoubleClicked(QTableWidgetItem* item);
    void onItemSelectionChanged();
    void onDeleteClicked();
    void showTableContextMenu(const QPoint& pos);

private:
    void retranslateUi();
    std::shared_ptr<FPSCMap> m_map;
    int m_currentFloor = 0;

    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QComboBox* m_traitCombo = nullptr;
    QCheckBox* m_currentFloorOnlyCheck = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_deleteBtn = nullptr;

    bool m_isUpdatingSelection = false;
};

#endif // ENTITYSEARCHDOCK_H
