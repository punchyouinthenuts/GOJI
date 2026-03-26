#include "monthcomboboxhelper.h"

#include <QComboBox>
#include <QChar>
#include <QString>

void MonthComboBoxHelper::populateWithBlankAndMonths(QComboBox* comboBox)
{
    if (!comboBox) {
        return;
    }

    comboBox->clear();
    comboBox->addItem(QString());

    for (int month = 1; month <= 12; ++month) {
        comboBox->addItem(QString("%1").arg(month, 2, 10, QChar('0')));
    }
}
