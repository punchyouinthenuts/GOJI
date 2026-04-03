#include "openjobmenuhelper.h"

#include <QAction>
#include <QHash>
#include <QObject>
#include <algorithm>
#include <limits>

namespace OpenJobMenuHelper {

int toIntOr(const QString& value, int fallback)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return ok ? parsed : fallback;
}

void buildMenu(QMenu* rootMenu, QObject* receiver, const QList<JobRow>& jobs, const BuildSpec& spec)
{
    if (!rootMenu) {
        return;
    }

    QList<JobRow> sortedJobs = jobs;

    const auto yearValue = [&](const JobRow& row) {
        return spec.yearSort ? spec.yearSort(row) : toIntOr(row.value("year"), -1);
    };
    const auto monthValue = [&](const JobRow& row) {
        return spec.monthSort ? spec.monthSort(row) : toIntOr(row.value("month"), -1);
    };
    const auto componentValue = [&](const JobRow& row) {
        return spec.componentSort ? spec.componentSort(row) : std::numeric_limits<int>::min();
    };
    const auto jobNumberValue = [&](const JobRow& row) {
        return spec.jobNumberSort ? spec.jobNumberSort(row) : toIntOr(row.value("job_number"), -1);
    };

    std::stable_sort(sortedJobs.begin(), sortedJobs.end(), [&](const JobRow& a, const JobRow& b) {
        const int ay = yearValue(a);
        const int by = yearValue(b);
        if (ay != by) return ay > by;

        const int am = monthValue(a);
        const int bm = monthValue(b);
        if (am != bm) return am > bm;

        const int ac = componentValue(a);
        const int bc = componentValue(b);
        if (ac != bc) return ac > bc;

        const int aj = jobNumberValue(a);
        const int bj = jobNumberValue(b);
        if (aj != bj) return aj > bj;

        return a.value("job_number") > b.value("job_number");
    });

    QHash<QString, QMenu*> yearMenus;
    QHash<QString, QMenu*> monthMenus;

    for (const JobRow& row : std::as_const(sortedJobs)) {
        if (spec.beforeAddAction) {
            spec.beforeAddAction(row);
        }

        const QString yearText = spec.yearMenuText ? spec.yearMenuText(row) : row.value("year");
        QMenu* yearMenu = yearMenus.value(yearText, nullptr);
        if (!yearMenu) {
            yearMenu = rootMenu->addMenu(yearText);
            yearMenus.insert(yearText, yearMenu);
        }

        QMenu* targetMenu = yearMenu;
        if (spec.groupByMonth) {
            const QString monthText = spec.monthMenuText ? spec.monthMenuText(row) : row.value("month");
            const QString monthKey = yearText + "|" + monthText;
            QMenu* monthMenu = monthMenus.value(monthKey, nullptr);
            if (!monthMenu) {
                monthMenu = yearMenu->addMenu(monthText);
                monthMenus.insert(monthKey, monthMenu);
            }
            targetMenu = monthMenu;
        }

        const QString text = spec.actionText ? spec.actionText(row) : row.value("job_number");
        QAction* action = targetMenu->addAction(text);

        if (spec.configureAction) {
            spec.configureAction(action, row);
        }

        if (spec.onTriggered) {
            QObject::connect(action, &QAction::triggered, receiver, [handler = spec.onTriggered, row]() {
                handler(row);
            });
        }
    }
}

} // namespace OpenJobMenuHelper
