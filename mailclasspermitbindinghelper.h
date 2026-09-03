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
                     QObject* context);

    static QString permitForClass(const QString& mailClass);
    static QString classForPermit(const QString& permit);
    static QString normalizePermitForUi(const QString& permit);
};

#endif // MAILCLASSPERMITBINDINGHELPER_H
