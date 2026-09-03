#include "mailclasspermitbindinghelper.h"

#include <QComboBox>
#include <QObject>
#include <QSignalBlocker>

QString MailClassPermitBindingHelper::permitForClass(const QString& mailClass)
{
    const QString normalized = mailClass.trimmed().toUpper();
    if (normalized == QStringLiteral("STANDARD")) {
        return QStringLiteral("1662");
    }
    if (normalized == QStringLiteral("FIRST CLASS")) {
        return QStringLiteral("METERED");
    }
    return QString();
}

QString MailClassPermitBindingHelper::classForPermit(const QString& permit)
{
    const QString normalized = permit.trimmed().toUpper();
    if (normalized == QStringLiteral("1662")) {
        return QStringLiteral("STANDARD");
    }
    if (normalized == QStringLiteral("METERED") || normalized == QStringLiteral("METER")) {
        return QStringLiteral("FIRST CLASS");
    }
    return QString();
}

QString MailClassPermitBindingHelper::normalizePermitForUi(const QString& permit)
{
    return permit.trimmed().compare(QStringLiteral("METER"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("METERED")
        : permit;
}

bool MailClassPermitBindingHelper::bind(QComboBox* classComboBox,
                                        QComboBox* permitComboBox,
                                        QObject* context)
{
    if (!classComboBox || !permitComboBox || !context) {
        return false;
    }

    QObject::connect(classComboBox, &QComboBox::currentTextChanged, context,
                     [permitComboBox](const QString& mailClass) {
                         const QString permit = permitForClass(mailClass);
                         if (permit.isEmpty() || permitComboBox->currentText() == permit) {
                             return;
                         }
                         const QSignalBlocker blocker(permitComboBox);
                         permitComboBox->setCurrentText(permit);
                     });

    QObject::connect(permitComboBox, &QComboBox::currentTextChanged, context,
                     [classComboBox, permitComboBox](const QString& rawPermit) {
                         const QString permit = normalizePermitForUi(rawPermit);
                         if (permit != rawPermit && permitComboBox->findText(permit) >= 0) {
                             const QSignalBlocker blocker(permitComboBox);
                             permitComboBox->setCurrentText(permit);
                         }

                         const QString mailClass = classForPermit(permit);
                         if (mailClass.isEmpty() || classComboBox->currentText() == mailClass) {
                             return;
                         }
                         const QSignalBlocker blocker(classComboBox);
                         classComboBox->setCurrentText(mailClass);
                     });

    return true;
}
