#include "yearcomboboxhelper.h"

#include <QComboBox>
#include <QDate>
#include <QString>

void YearComboBoxHelper::populateWithBlankAndAdjacentYears(QComboBox* comboBox)
{
    if (!comboBox) {
        return;
    }

    comboBox->clear();
    comboBox->addItem(QString());

    const int currentYear = QDate::currentDate().year();

    comboBox->addItem(QString::number(currentYear - 1));
    comboBox->addItem(QString::number(currentYear));
    comboBox->addItem(QString::number(currentYear + 1));
}
