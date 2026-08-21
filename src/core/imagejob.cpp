#include "core/imagejob.h"

#include <QFileInfo>

namespace papercutter {

QString ImageJob::displayName() const
{
    return QFileInfo(sourcePath).fileName();
}

} // namespace papercutter

