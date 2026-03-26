#include "dropbindinghelper.h"

#include "dropwindow.h"

#include <QObject>

bool DropBindingHelper::setupDropWindow(
    DropWindow* dropWindow,
    const QString& targetDirectory,
    const QStringList& supportedExtensions,
    QObject* context,
    const FilesDroppedHandler& filesDroppedHandler,
    const FileDropErrorHandler& fileDropErrorHandler)
{
    if (!dropWindow || !context || !filesDroppedHandler || !fileDropErrorHandler) {
        return false;
    }

    dropWindow->setTargetDirectory(targetDirectory);
    dropWindow->setSupportedExtensions(supportedExtensions);

    QObject::connect(dropWindow, &DropWindow::filesDropped, context,
                     [filesDroppedHandler](const QStringList& filePaths) {
                         filesDroppedHandler(filePaths);
                     });
    QObject::connect(dropWindow, &DropWindow::fileDropError, context,
                     [fileDropErrorHandler](const QString& errorMessage) {
                         fileDropErrorHandler(errorMessage);
                     });

    dropWindow->clearFiles();

    return true;
}
