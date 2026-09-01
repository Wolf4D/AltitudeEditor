#ifndef SEGMENTPARSER_H
#define SEGMENTPARSER_H

#include "FPSCData.h"
#include <QString>
#include <memory>

class SegmentParser {
public:
    static std::shared_ptr<FPSCSegment> parse(const QString& relPath, int segId);
};

#endif // SEGMENTPARSER_H
