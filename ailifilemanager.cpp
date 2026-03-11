#include "ailifilemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

AILIFileManager::AILIFileManager(QObject *parent)
    : QObject(parent)
{
    m_basePath = "C:/Goji/AUTOMATION/AILI";
    m_originalPath = m_basePath + "/ORIGINAL";
    m_inputPath = m_basePath + "/INPUT";
    m_outputPath = m_basePath + "/OUTPUT";
    m_archivePath = m_basePath + "/ARCHIVE";
}

bool AILIFileManager::initializeDirectories()
{
    QDir dir;

    if (!dir.exists(m_basePath))
        dir.mkpath(m_basePath);

    if (!dir.exists(m_originalPath))
        dir.mkpath(m_originalPath);

    if (!dir.exists(m_inputPath))
        dir.mkpath(m_inputPath);

    if (!dir.exists(m_outputPath))
        dir.mkpath(m_outputPath);

    if (!dir.exists(m_archivePath))
        dir.mkpath(m_archivePath);

    return true;
}

QString AILIFileManager::basePath() const
{
    return m_basePath;
}

QString AILIFileManager::originalPath() const
{
    return m_originalPath;
}

QString AILIFileManager::inputPath() const
{
    return m_inputPath;
}

QString AILIFileManager::outputPath() const
{
    return m_outputPath;
}

QString AILIFileManager::archivePath() const
{
    return m_archivePath;
}

bool AILIFileManager::copyOriginalFile(const QString &sourceFilePath, QString &destinationPath)
{
    QFileInfo sourceInfo(sourceFilePath);

    if (!sourceInfo.exists())
    {
        qWarning() << "AILI: source file does not exist:" << sourceFilePath;
        return false;
    }

    QString destination = QDir(m_originalPath).filePath(sourceInfo.fileName());
    QFileInfo destinationInfo(destination);

    QString sourceCanonical = sourceInfo.canonicalFilePath();
    QString destinationCanonical = destinationInfo.canonicalFilePath();

    if (!sourceCanonical.isEmpty() && !destinationCanonical.isEmpty()
        && sourceCanonical.compare(destinationCanonical, Qt::CaseInsensitive) == 0)
    {
        destinationPath = destination;
        return true;
    }

    const QString sourceAbsolute = QDir::cleanPath(sourceInfo.absoluteFilePath());
    const QString destinationAbsolute = QDir::cleanPath(destinationInfo.absoluteFilePath());
    if (sourceAbsolute.compare(destinationAbsolute, Qt::CaseInsensitive) == 0)
    {
        destinationPath = destination;
        return true;
    }

    if (QFile::exists(destination))
    {
        if (!QFile::remove(destination))
        {
            qWarning() << "AILI: failed removing existing file in ORIGINAL";
            return false;
        }
    }

    if (!QFile::copy(sourceFilePath, destination))
    {
        qWarning() << "AILI: failed copying file to ORIGINAL";
        return false;
    }

    destinationPath = destination;
    return true;
}

QString AILIFileManager::findInvalidAddressFile() const
{
    QStringList patterns;
    patterns << "AIL - INVALID ADDRESS RECORDS.csv";

    return findFileMatching(m_outputPath, patterns);
}

QString AILIFileManager::findDomesticOutputFile(const QString &version) const
{
    QStringList patterns;

    if (version == "AO SPOTLIGHT")
    {
        patterns << "AO SPOTLIGHT DOMESTIC.csv";
    }
    else
    {
        patterns << "SPOTLIGHT DOMESTIC.csv";
    }

    return findFileMatching(m_outputPath, patterns);
}

QString AILIFileManager::findInternationalFile(const QString &version) const
{
    QStringList patterns;

    if (version == "AO SPOTLIGHT")
    {
        patterns << "AO SL INTERNATIONAL.csv";
    }
    else
    {
        patterns << "SL INTERNATIONAL.csv";
    }

    return findFileMatching(m_inputPath, patterns);
}

QStringList AILIFileManager::listInputFiles() const
{
    QDir dir(m_inputPath);
    return dir.entryList(QDir::Files);
}

QStringList AILIFileManager::listOutputFiles() const
{
    QDir dir(m_outputPath);
    return dir.entryList(QDir::Files);
}

bool AILIFileManager::fileExists(const QString &filePath) const
{
    QFileInfo info(filePath);
    return info.exists();
}

QString AILIFileManager::findFileMatching(const QString &directory,
                                          const QStringList &patterns) const
{
    QDir dir(directory);

    if (!dir.exists())
        return QString();

    QStringList files = dir.entryList(QDir::Files);

    for (const QString &file : files)
    {
        for (const QString &pattern : patterns)
        {
            if (file.compare(pattern, Qt::CaseInsensitive) == 0)
            {
                return dir.absoluteFilePath(file);
            }
        }
    }

    return QString();
}
