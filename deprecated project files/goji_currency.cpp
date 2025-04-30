#include "goji.h"
#include <QLineEdit>
#include <QLocale>
#include <QRegExp>

void Goji::formatCurrencyOnFinish()
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) return;

    QString text = lineEdit->text().trimmed();
    if (text.isEmpty()) {
        lineEdit->clear();
        return;
    }

    text.remove(QRegExp("[^0-9.]"));
    if (text.isEmpty()) {
        lineEdit->clear();
        return;
    }

    bool ok;
    double value = text.toDouble(&ok);
    if (!ok) {
        lineEdit->clear();
        return;
    }

    QLocale locale(QLocale::English, QLocale::UnitedStates);
    QString formatted = locale.toString(value, 'f', 2);
    formatted.prepend("$");
    lineEdit->setText(formatted);
}
