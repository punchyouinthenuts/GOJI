#include "basefilesystemmanager.h"
#include "logger.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

bool BaseFileSystemManager::createDirectoryIfNotExists(const QString& path)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }

    Logger::instance().info(QString("Creating directory: %1").arg(path));
    if (dir.mkpath(".")) {
        return true;
    }

    Logger::instance().error(QString("Failed to create directory: %1").arg(path));
    return false;
}

bool BaseFileSystemManager::copyFile(const QString& source, const QString& destination)
{
    // Check if source exists
    if (!fileExists(source)) {
        Logger::instance().error(QString("Source file does not exist: %1").arg(source));
        return false;
    }

    // Create destination directory if it doesn't exist
    QFileInfo destInfo(destination);
    QDir destDir = destInfo.dir();
    if (!destDir.exists() && !destDir.mkpath(".")) {
        Logger::instance().error(QString("Failed to create destination directory: %1").arg(destDir.path()));
        return false;
    }

    // Remove destination file if it exists
    if (fileExists(destination) && !QFile::remove(destination)) {
        Logger::instance().error(QString("Failed to remove existing destination file: %1").arg(destination));
        return false;
    }

    // Copy the file
    if (QFile::copy(source, destination)) {
        Logger::instance().info(QString("Copied file from %1 to %2").arg(source, destination));
        m_completedOperations.append(qMakePair(source, destination));
        return true;
    }

    Logger::instance().error(QString("Failed to copy file from %1 to %2").arg(source, destination));
    return false;
}

bool BaseFileSystemManager::moveFile(const QString& source, const QString& destination)
{
    // First try to copy the file
    if (!copyFile(source, destination)) {
        return false;
    }

    // Then remove the source file
    if (QFile::remove(source)) {
        Logger::instance().info(QString("Moved file from %1 to %2").arg(source, destination));
        return true;
    }

    Logger::instance().error(QString("Failed to remove source file after copy: %1").arg(source));
    return false;
}

bool BaseFileSystemManager::fileExists(const QString& path) const
{
    return QFile::exists(path);
}

QStringList BaseFileSystemManager::getFilesInDirectory(const QString& dirPath, const QString& filter) const
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        Logger::instance().warning(QString("Directory does not exist: %1").arg(dirPath));
        return QStringList();
    }

    return dir.entryList(QStringList() << filter, QDir::Files, QDir::Name);
}

bool BaseFileSystemManager::openFile(const QString& filePath) const
{
    if (!fileExists(filePath)) {
        Logger::instance().error(QString("File does not exist: %1").arg(filePath));
        return false;
    }

    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        Logger::instance().info(QString("Opened file: %1").arg(filePath));
        return true;
    }

    Logger::instance().error(QString("Failed to open file: %1").arg(filePath));
    return false;
}
