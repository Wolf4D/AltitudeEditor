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
    void clear();

private:
    void addProperty(QTreeWidgetItem* parent, const QString& name, const QString& value, const QString& tip = QString());

    QTreeWidget* m_tree = nullptr;
    QLabel* m_headerLabel = nullptr;
};

#endif // ENTITYINSPECTOR_H
