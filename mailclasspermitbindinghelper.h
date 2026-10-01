#ifndef MAILCLASSPERMITBINDINGHELPER_H
#define MAILCLASSPERMITBINDINGHELPER_H

#include <QString>

class QComboBox;
class QObject;

class MailClassPermitBindingHelper
{
public:
    static bool bind(QComboBox* classComboBox,
                     QComboBox* permitComboBox,
                     QObject* context,
                     const QString& meterPermitLabel = QStringLiteral("METER"));

    static QString permitForClass(
        const QString& mailClass,
        const QString& meterPermitLabel = QStringLiteral("METER"));
    static QString classForPermit(const QString& permit);
    static QString normalizePermitForUi(
        const QString& permit,
        const QString& meterPermitLabel = QStringLiteral("METER"));
};

#endif // MAILCLASSPERMITBINDINGHELPER_H
