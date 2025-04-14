#include "goji.h"
#include "ui_GOJI.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>

void Goji::logToTerminal(const QString &message)
{
    ui->terminalWindow->append(message);
    ui->terminalWindow->verticalScrollBar()->setValue(
        ui->terminalWindow->verticalScrollBar()->maximum());
}

void Goji::clearJobNumbers()
{
    ui->cbcJobNumber->clear();
    ui->ncwoJobNumber->clear();
    ui->inactiveJobNumber->clear();
    ui->prepifJobNumber->clear();
    ui->excJobNumber->clear();
}

void Goji::onPrintDirChanged(const QString &path)
{
    Q_UNUSED(path);
    logToTerminal("Print directory changed: " + path);
}

void Goji::checkAllPrintFilesReady()
{
    logToTerminal("Checking if all print files are ready...");
}

void Goji::checkProofFiles()
{
    logToTerminal("Checking proof files...");
}

QString Goji::getProofFolderPath(const QString &jobType)
{
    Q_UNUSED(jobType);
    return QString();
}

void Goji::initializePrintFileMonitoring()
{
    logToTerminal("Initializing print file monitoring...");
}

void Goji::runScript(const QString &program, const QStringList &arguments)
{
    QProcess *process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        ui->terminalWindow->append(process->readAllStandardOutput());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        ui->terminalWindow->append("<font color=\"red\">" + process->readAllStandardError() + "</font>");
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    ui->terminalWindow->append("Script completed successfully.");
                } else {
                    ui->terminalWindow->append("Script failed with exit code " + QString::number(exitCode));
                }
                process->deleteLater();
            });
    process->start(program, arguments);
}

void Goji::ensureInDesignIsOpen(const std::function<void()>& callback)
{
    QString inDesignPath = "C:\\Program Files\\Adobe\\Adobe InDesign 2024\\InDesign.exe";
    QProcess::startDetached(inDesignPath, QStringList());
    QTimer::singleShot(20000, this, callback);
}

void Goji::openProofFiles(const QString& selection)
{
    if (!proofFiles.contains(selection)) {
        logToTerminal("Invalid selection: " + selection);
        return;
    }
    QString basePath = QCoreApplication::applicationDirPath();
    const QStringList& fileList = proofFiles[selection];
    for (const QString& relativePath : fileList) {
        QString fullPath = basePath + relativePath;
        if (QFile::exists(fullPath)) {
            logToTerminal("Opening file: " + fullPath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
        } else {
            logToTerminal("File not found: " + fullPath);
        }
    }
}

void Goji::openPrintFiles(const QString& selection)
{
    if (!printFiles.contains(selection)) {
        logToTerminal("Invalid selection: " + selection);
        return;
    }
    QString basePath = QCoreApplication::applicationDirPath();
    const QStringList& fileList = printFiles[selection];
    for (const QString& relativePath : fileList) {
        QString fullPath = basePath + relativePath;
        if (QFile::exists(fullPath)) {
            logToTerminal("Opening file: " + fullPath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
        } else {
            logToTerminal("File not found: " + fullPath);
        }
    }
}

void Goji::lockJobDataFields(bool lock)
{
    ui->cbcJobNumber->setReadOnly(lock);
    ui->ncwoJobNumber->setReadOnly(lock);
    ui->inactiveJobNumber->setReadOnly(lock);
    ui->prepifJobNumber->setReadOnly(lock);
    ui->excJobNumber->setReadOnly(lock);
    ui->yearDDbox->setEnabled(!lock);
    ui->monthDDbox->setEnabled(!lock);
    ui->weekDDbox->setEnabled(!lock);
}

void Goji::updateLEDs()
{
    ui->preProofLED->setStyleSheet(isRunPreProofComplete ? "background-color: #00ff15;" : "background-color: red;");
    ui->proofFilesLED->setStyleSheet(isOpenProofFilesComplete ? "background-color: #00ff15;" : "background-color: red;");
    ui->postProofLED->setStyleSheet(isRunPostProofComplete ? "background-color: #00ff15;" : "background-color: red;");
    ui->printFilesLED->setStyleSheet(isOpenPrintFilesComplete ? "background-color: #00ff15;" : "background-color: red;");
    ui->postPrintLED->setStyleSheet(isRunPostPrintComplete ? "background-color: #00ff15;" : "background-color: red;");
}

void Goji::updateButtonStates(bool enabled)
{
    ui->openIZ->setEnabled(enabled);
    ui->runInitial->setEnabled(enabled);
    ui->runPreProof->setEnabled(enabled);
    ui->openPrintFiles->setEnabled(enabled);
    ui->runPostPrint->setEnabled(enabled);
}

void Goji::enableProofApprovalCheckboxes()
{
    ui->cbcCB->setEnabled(true);
    ui->excCB->setEnabled(true);
    ui->inactiveCB->setEnabled(true);
    ui->ncwoCB->setEnabled(true);
    ui->prepifCB->setEnabled(true);
    ui->allCB->setEnabled(true);
}

int Goji::getNextProofVersion(const QString& filePath)
{
    QSqlQuery query;
    query.prepare("SELECT version FROM proof_versions WHERE file_path = :filePath");
    query.bindValue(":filePath", filePath);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() + 1;
    } else {
        query.prepare("INSERT OR IGNORE INTO proof_versions (file_path, version) VALUES (:filePath, 1)");
        query.bindValue(":filePath", filePath);
        if (!query.exec()) {
            qDebug() << "Insert version error:" << query.lastError().text();
        }
        return 2; // Next version after initial v1
    }
}

void Goji::regenerateProofs()
{
    QString basePath = QCoreApplication::applicationDirPath();
    QStringList filesToZip;
    QString zipFileName = "Regenerated_Proofs_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".zip";
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/regenerate_proofs.py";

    // Collect selected proofs
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        if (it.value()->isChecked()) {
            QString jobType = it.key();
            if (proofFiles.contains(jobType)) {
                const QStringList& fileList = proofFiles[jobType];
                for (const QString& relativePath : fileList) {
                    QString fullPath = basePath + relativePath;
                    if (QFile::exists(fullPath)) {
                        int nextVersion = getNextProofVersion(fullPath);
                        QString versionedPath = fullPath.left(fullPath.lastIndexOf('.')) + "_v" + QString::number(nextVersion) + ".indd";
                        QFile::copy(fullPath, versionedPath);
                        filesToZip << versionedPath;

                        // Update version in database
                        QSqlQuery query;
                        query.prepare("INSERT OR REPLACE INTO proof_versions (file_path, version) VALUES (:filePath, :version)");
                        query.bindValue(":filePath", fullPath);
                        query.bindValue(":version", nextVersion);
                        if (!query.exec()) {
                            qDebug() << "Update version error:" << query.lastError().text();
                        }
                    }
                }
            }
        }
    }

    if (filesToZip.isEmpty()) {
        logToTerminal("No proofs selected for regeneration.");
        return;
    }

    // Generate Python script to zip files
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << "#!/usr/bin/env python3\n";
        out << "import zipfile\n";
        out << "import os\n\n";
        out << "def zip_files():\n";
        out << "    zip_name = '" << zipFileName << "'\n";
        out << "    files = " << QString("['%1']").arg(filesToZip.join("','")) << "\n";
        out << "    with zipfile.ZipFile(zip_name, 'w', zipfile.ZIP_DEFLATED) as zipf:\n";
        out << "        for file in files:\n";
        out << "            zipf.write(file, os.path.basename(file))\n";
        out << "    print('Zipped files into', zip_name)\n\n";
        out << "if __name__ == '__main__':\n";
        out << "    zip_files()\n";
        scriptFile.close();
    } else {
        logToTerminal("Failed to create regeneration script.");
        return;
    }

    // Run the script
    runScript("python", {scriptPath});
    logToTerminal("Proof regeneration complete. Files zipped as " + zipFileName);
    isRunPostProofComplete = true;
    updateLEDs();
}
