#ifndef FPMWRITER_H
#define FPMWRITER_H

#include "FPSCData.h"
#include <QString>
#include <QByteArray>
#include <memory>

class FPMWriter {
public:
    static bool saveMap(
        const std::shared_ptr<FPSCMap>& map,
        const QString& targetPath,
        const QString& password = QStringLiteral("mypassword")
    );
};

#endif // FPMWRITER_H
