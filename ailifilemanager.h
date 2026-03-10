#ifndef AILIFILEMANAGER_H
#define AILIFILEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

/*
 * AILIFileManager
 *
 * Responsible for all filesystem operations used by the AILI tab workflow.
 * This includes folder initialization, file discovery, and path generation.
 *
 * Folder structure managed:
 *
 * C:\Goji\AUTOMATION\AILI\
 *      ORIGINAL
 *      INPUT
 *      OUTPUT
 *      ARCHIVE
 *
 * ORIGINAL
 *      Stores the original XLSX dropped into the drop window.
 *
 * INPUT
 *      Stores CSV files produced by Script 01.
 *
 * OUTPUT
 *      Stores Bulk Mailer output files.
 *
 * ARCHIVE
 *      Stores archived job packages created during final processing.
 */

class AILIFileManager : public QObject
{
    Q_OBJECT

public:
    explicit AILIFileManager(QObject *parent = nullptr);

    bool initializeDirectories();

    QString basePath() const;

    QString originalPath() const;
    QString inputPath() const;
    QString outputPath() const;
    QString archivePath() const;

    bool copyOriginalFile(const QString &sourceFilePath, QString &destinationPath);

    QString findInvalidAddressFile() const;

    QString findDomesticOutputFile(const QString &version) const;

    QString findInternationalFile(const QString &version) const;

    QStringList listInputFiles() const;
    QStringList listOutputFiles() const;

    bool fileExists(const QString &filePath) const;

private:

    QString m_basePath;
    QString m_originalPath;
    QString m_inputPath;
    QString m_outputPath;
    QString m_archivePath;

    QString findFileMatching(const QString &directory,
                             const QStringList &patterns) const;
};

#endif // AILIFILEMANAGER_H
