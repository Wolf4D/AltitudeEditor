#ifndef ENTITYPARSER_H
#define ENTITYPARSER_H

#include "FPSCData.h"
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <memory>

class EntityParser {
public:
    static std::shared_ptr<FPSCEntityProfile> parseProfile(const QString& relPath, int bankId);
    static QVector<PlacedEntity> parseMapEle(
        const QByteArray& eleData,
        const QVector<QString>& entBank,
        const QMap<int, std::shared_ptr<FPSCEntityProfile>>& profiles
    );
};

#endif // ENTITYPARSER_H
