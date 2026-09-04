#ifndef ENTITYINSPECTOR_H
#define ENTITYINSPECTOR_H

#include "FPSCData.h"
#include <QDockWidget>
#include <QTreeWidget>
#include <QLabel>
#include <memory>

class EntityInspector : public QDockWidget {
    Q_OBJECT
public:
    explicit EntityInspector(QWidget* parent = nullptr);
    ~EntityInspector() override = default;

    void setEntity(std::shared_ptr<FPSCMap> map, int index);
    void refreshValues();
    void clear();

signals:
    void entityModified(int index);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void addProperty(QTreeWidgetItem* parent, const QString& name, const QString& value, const QString& tip = QString());
    void addWidgetProperty(QTreeWidgetItem* parent, const QString& name, QWidget* widget, const QString& tip = QString());

    QTreeWidget* m_tree = nullptr;
    QLabel* m_headerLabel = nullptr;
    QObject* m_noWheelFilter = nullptr;

    class QDoubleSpinBox* m_spinX = nullptr;
    class QDoubleSpinBox* m_spinY = nullptr;
    class QDoubleSpinBox* m_spinZ = nullptr;

    std::shared_ptr<FPSCMap> m_map;
    int m_currentIndex = -1;
    bool m_isPopulating = false;
};

#endif // ENTITYINSPECTOR_H
