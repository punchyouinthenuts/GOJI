#ifndef DROPBINDINGHELPER_H
#define DROPBINDINGHELPER_H

#include <QString>
#include <QStringList>
#include <functional>

class QObject;
class DropWindow;

class DropBindingHelper
{
public:
    using FilesDroppedHandler = std::function<void(const QStringList&)>;
    using FileDropErrorHandler = std::function<void(const QString&)>;

    static bool setupDropWindow(
        DropWindow* dropWindow,
        const QString& targetDirectory,
        const QStringList& supportedExtensions,
        QObject* context,
        const FilesDroppedHandler& filesDroppedHandler,
        const FileDropErrorHandler& fileDropErrorHandler);
};

#endif // DROPBINDINGHELPER_H
