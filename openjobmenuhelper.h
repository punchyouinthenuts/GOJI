#ifndef OPENJOBMENUHELPER_H
#define OPENJOBMENUHELPER_H

#include <QList>
#include <QMap>
#include <QMenu>
#include <QString>
#include <functional>

class QAction;
class QObject;

namespace OpenJobMenuHelper {

using JobRow = QMap<QString, QString>;

int toIntOr(const QString& value, int fallback);

struct BuildSpec {
    bool groupByMonth = false;
    std::function<int(const JobRow&)> yearSort;
    std::function<int(const JobRow&)> monthSort;
    std::function<int(const JobRow&)> componentSort;
    std::function<int(const JobRow&)> jobNumberSort;
    std::function<QString(const JobRow&)> yearMenuText;
    std::function<QString(const JobRow&)> monthMenuText;
    std::function<QString(const JobRow&)> actionText;
    std::function<void(QAction*, const JobRow&)> configureAction;
    std::function<void(const JobRow&)> beforeAddAction;
    std::function<void(const JobRow&)> onTriggered;
};

void buildMenu(QMenu* rootMenu, QObject* receiver, const QList<JobRow>& jobs, const BuildSpec& spec);

} // namespace OpenJobMenuHelper

#endif // OPENJOBMENUHELPER_H
