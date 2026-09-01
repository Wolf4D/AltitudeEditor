#ifndef FPMREADER_H
#define FPMREADER_H

#include "FPSCData.h"
#include <QString>
#include <QByteArray>
#include <QMap>
#include <memory>

class FPMReader {
public:
    static std::shared_ptr<FPSCMap> loadMap(const QString& fpmPath, const QString& password = "mypassword");

private:
    static bool extractZipEntries(
        const QString& zipPath,
        const QString& password,
        QMap<QString, QByteArray>& outEntries
    );
};

#endif // FPMREADER_H
