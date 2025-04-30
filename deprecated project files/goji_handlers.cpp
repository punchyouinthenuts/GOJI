#include "goji.h"
#include "ui_GOJI.h"
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QMap>
#include <QStringList>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDialog>
#include <QClipboard>
#include <QApplication>

// Button Handler Implementations for QPushButton
void Goji::onOpenIZClicked()
{
    logToTerminal("Opening InputZIP directory...");
    QString inputZipPath = QCoreApplication::applicationDirPath() + "/RAC/WEEKLY/INPUTZIP";
    QDesktopServices::openUrl(QUrl::fromLocalFile(inputZipPath));
    isOpenIZComplete = true;
    updateLEDs();
}

void Goji::onRunInitialClicked()
{
    if (!isOpenIZComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please open InputZIP first.");
        return;
    }
    logToTerminal("Running Initial Script...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/01RUNFIRST.py";
    runScript("python", {scriptPath});
    isRunInitialComplete = true;
    updateLEDs();
}

void Goji::onRunPreProofClicked()
{
    if (!isRunInitialComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please run Initial Script first.");
        return;
    }
    if (!isPostageLocked) {
        QMessageBox::warning(this, "Postage Not Locked", "Please lock the postage data first.");
        return;
    }

    QMap<QString, QStringList> requiredFiles;
    requiredFiles["CBC"] = QStringList() << "CBC2_WEEKLY.csv" << "CBC3_WEEKLY.csv";
    requiredFiles["EXC"] = QStringList() << "EXC_OUTPUT.csv";
    requiredFiles["INACTIVE"] = QStringList() << "A-PO.txt" << "A-PU.txt";
    requiredFiles["NCWO"] = QStringList() << "1-A_OUTPUT.csv" << "1-AP_OUTPUT.csv" << "2-A_OUTPUT.csv" << "2-AP_OUTPUT.csv";
    requiredFiles["PREPIF"] = QStringList() << "PRE_PIF.csv";

    QString basePath = QCoreApplication::applicationDirPath();
    QStringList missingFiles;
    for (auto it = requiredFiles.constBegin(); it != requiredFiles.constEnd(); ++it) {
        QString jobType = it.key();
        QString outputDir = basePath + "/RAC/" + jobType + "/JOB/OUTPUT";
        for (const QString& fileName : it.value()) {
            QString filePath = outputDir + "/" + fileName;
            if (!QFile::exists(filePath)) {
                missingFiles.append(fileName);
            }
        }
    }

    if (!missingFiles.isEmpty()) {
        QString message = "The following data files are missing from their OUTPUT folders:\n\n";
        message += missingFiles.join("\n");
        message += "\n\nDo you want to proceed?";
        int choice = QMessageBox::warning(this, "Missing Files", message, QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            return;
        }

        QMessageBox confirmBox;
        confirmBox.setText("CONFIRM INCOMPLETE CONTINUE");
        QPushButton *confirmButton = confirmBox.addButton("Confirm", QMessageBox::AcceptRole);
        confirmBox.addButton("Cancel", QMessageBox::RejectRole);
        confirmBox.exec();
        if (confirmBox.clickedButton() != confirmButton) {
            return;
        }
    }

    logToTerminal("Running Pre-Proof...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/02RUNSECOND.bat";
    QStringList arguments = {scriptPath, basePath, ui->cbcJobNumber->text(), ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText()};
    runScript("cmd.exe", QStringList() << "/c" << arguments);
    isRunPreProofComplete = true;
    updateLEDs();
}

void Goji::onOpenProofFilesClicked()
{
    if (!isRunPreProofComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please run Pre-Proof first.");
        return;
    }
    QString selection = ui->proofDDbox->currentText();
    if (selection.isEmpty()) {
        logToTerminal("Please select a job type from proofDDbox.");
        return;
    }
    logToTerminal("Checking if Adobe InDesign is available...");
    ensureInDesignIsOpen([this, selection]() {
        openProofFiles(selection);
        isOpenProofFilesComplete = true;
        updateLEDs();
    });
}

void Goji::onRunPostProofClicked()
{
    if (!isOpenProofFilesComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please open proof files first.");
        return;
    }

    // Define expected proof files for each job type
    QMap<QString, QStringList> expectedProofFiles;
    expectedProofFiles["CBC"] = QStringList() << "CBC2 PROOF.pdf" << "CBC3 PROOF.pdf";
    expectedProofFiles["EXC"] = QStringList() << "EXC PROOF.pdf";
    expectedProofFiles["INACTIVE"] = QStringList() << "INACTIVE A-PO PROOF.pdf" << "INACTIVE A-PU PROOF.pdf" << "INACTIVE AT-PO PROOF.pdf"
                                                   << "INACTIVE AT-PU PROOF.pdf" << "INACTIVE PR-PO PROOF.pdf" << "INACTIVE PR-PU PROOF.pdf";
    expectedProofFiles["NCWO"] = QStringList() << "NCWO 1-A PROOF.pdf" << "NCWO 1-AP PROOF.pdf" << "NCWO 1-APPR PROOF.pdf" << "NCWO 1-PR PROOF.pdf"
                                               << "NCWO 2-A PROOF.pdf" << "NCWO 2-AP PROOF.pdf" << "NCWO 2-APPR PROOF.pdf" << "NCWO 2-PR PROOF.pdf";
    expectedProofFiles["PREPIF"] = QStringList() << "PREPIF US PROOF.pdf" << "PREPIF PR PROOF.pdf";

    // List of job types to check
    QStringList jobTypesToCheck = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};

    // Base path for proof files
    QString basePath = QCoreApplication::applicationDirPath();

    // Check for missing proof files
    QStringList missingFiles;
    for (const QString& jobType : jobTypesToCheck) {
        QString proofDir = basePath + "/RAC/" + jobType + "/JOB/PROOF";
        for (const QString& file : expectedProofFiles[jobType]) {
            QString filePath = proofDir + "/" + file;
            if (!QFile::exists(filePath)) {
                missingFiles.append(filePath);
            }
        }
    }

    // Display warning if files are missing
    if (!missingFiles.isEmpty()) {
        QString message = "The following proof files are missing:\n\n" + missingFiles.join("\n") +
                          "\n\nDo you want to proceed anyway?";
        int choice = QMessageBox::warning(this, "Missing Proof Files", message,
                                          QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            return; // Exit if user chooses not to proceed
        }
    }

    // Proceed with post-proof process
    logToTerminal("Running Post-Proof...");
    if (isProofRegenMode) {
        regenerateProofs();
    } else {
        QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/04POSTPROOF.py";
        QString basePath = QCoreApplication::applicationDirPath();
        QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();

        QStringList arguments = {
            scriptPath,
            "--base_path", basePath,
            "--week", week,
            "--cbc_job", ui->cbcJobNumber->text(),
            "--exc_job", ui->excJobNumber->text(),
            "--inactive_job", ui->inactiveJobNumber->text(),
            "--ncwo_job", ui->ncwoJobNumber->text(),
            "--prepif_job", ui->prepifJobNumber->text(),
            "--cbc2_postage", ui->cbc2Postage->text(),
            "--cbc3_postage", ui->cbc3Postage->text(),
            "--exc_postage", ui->excPostage->text(),
            "--inactive_po_postage", ui->inactivePOPostage->text(),
            "--inactive_pu_postage", ui->inactivePUPostage->text(),
            "--ncwo_1a_postage", ui->ncwo1APostage->text(),
            "--ncwo_1ap_postage", ui->ncwo1APPostage->text(),
            "--ncwo_2a_postage", ui->ncwo2APostage->text(),
            "--ncwo_2ap_postage", ui->ncwo2APPostage->text(),
            "--prepif_postage", ui->prepifPostage->text()
        };

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
                        savePostProofCounts();
                        isRunPostProofComplete = true;
                        enableProofApprovalCheckboxes();
                        updateLEDs();
                    } else {
                        ui->terminalWindow->append("Script failed with exit code " + QString::number(exitCode));
                    }
                    process->deleteLater();
                });
        process->start("python", arguments);
    }
}

void Goji::onOpenPrintFilesClicked()
{
    if (!isRunPostProofComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please run Post-Proof first.");
        return;
    }
    QString selection = ui->printDDbox->currentText();
    if (selection.isEmpty()) {
        logToTerminal("Please select a job type from printDDBox.");
        return;
    }
    logToTerminal("Checking if Adobe InDesign is available...");
    ensureInDesignIsOpen([this, selection]() {
        openPrintFiles(selection);
        isOpenPrintFilesComplete = true;
        updateLEDs();
    });
}

void Goji::onRunPostPrintClicked()
{
    if (!isOpenPrintFilesComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please open print files first.");
        return;
    }
    logToTerminal("Running Post-Print...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/05POSTPRINT.ps1";
    runScript("powershell.exe", {"-ExecutionPolicy", "Bypass", "-File", scriptPath});
    isRunPostPrintComplete = true;
    updateLEDs();
}

// Button Handler Implementations for QToolButton
void Goji::onLockButtonToggled(bool checked)
{
    if (checked) {
        if (!isJobSaved) {
            if (jobExists(ui->yearDDbox->currentText(), ui->monthDDbox->currentText(), ui->weekDDbox->currentText())) {
                QMessageBox::warning(this, "Job Exists", "A job with this year, month, and week already exists.");
                ui->lockButton->setChecked(false);
                return;
            }
            insertJob();
            isJobSaved = true;
            originalYear = ui->yearDDbox->currentText();
            originalMonth = ui->monthDDbox->currentText();
            originalWeek = ui->weekDDbox->currentText();
            logToTerminal("Job Data Saved and Locked");
        } else {
            logToTerminal("Job Data Already Saved");
        }
        lockJobDataFields(true);
    } else {
        if (isJobSaved) {
            QMessageBox::warning(this, "Job Saved", "The job is already saved and cannot be unlocked.");
            ui->lockButton->setChecked(true);
        } else {
            lockJobDataFields(false);
            logToTerminal("Job Data Unlocked");
        }
    }
}

void Goji::onEditButtonToggled(bool checked)
{
    if (!isJobSaved) {
        if (checked) {
            QMessageBox::warning(this, "No Job Saved", "Cannot edit before saving the job.");
            ui->editButton->setChecked(false);
        }
        return;
    }

    if (checked) {
        lockJobDataFields(false);
        logToTerminal("Edit Mode Enabled");
        ui->editLabel->setText("EDITING ENABLED");
    } else {
        QString newYear = ui->yearDDbox->currentText();
        QString newMonth = ui->monthDDbox->currentText();
        QString newWeek = ui->weekDDbox->currentText();
        if (newYear != originalYear || newMonth != originalMonth || newWeek != originalWeek) {
            if (jobExists(newYear, newMonth, newWeek)) {
                QMessageBox::warning(this, "Job Exists", "JOB " + newMonth + "." + newWeek + " ALREADY EXISTS\n"
                                                                                             "In order to change details for " + newMonth + "." + newWeek + " open it from the menu.");
                ui->yearDDbox->setCurrentText(originalYear);
                ui->monthDDbox->setCurrentText(originalMonth);
                ui->weekDDbox->setCurrentText(originalWeek);
            } else {
                deleteJob(originalYear, originalMonth, originalWeek);
                insertJob();
                originalYear = newYear;
                originalMonth = newMonth;
                originalWeek = newWeek;
            }
        } else {
            updateJob();
        }
        lockJobDataFields(true);
        logToTerminal("Edit Mode Disabled");
        ui->editLabel->setText("EDITING DISABLED");
    }
}

void Goji::onProofRegenToggled(bool checked)
{
    isProofRegenMode = checked;
    if (checked) {
        logToTerminal("Proof Regeneration Mode Enabled");
        updateButtonStates(false);
        for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
            it.value()->setEnabled(true);
        }
    } else {
        logToTerminal("Proof Regeneration Mode Disabled");
        updateButtonStates(true);
        for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
            it.value()->setEnabled(false);
            it.value()->setChecked(false);
        }
    }
}

void Goji::onPostageLockToggled(bool checked)
{
    if (checked) {
        isPostageLocked = true;
        QList<QLineEdit*> postageLineEdits = {
            ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
            ui->inactivePOPostage, ui->inactivePUPostage,
            ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
        };
        for (QLineEdit *edit : postageLineEdits) {
            edit->setReadOnly(true);
        }
        logToTerminal("Postage Data Locked");
    } else {
        if (isRunPreProofComplete) {
            int result = QMessageBox::warning(this, "Warning", "Proof and postage data has already been processed.\n"
                                                               "Editing will require running Pre-Proof again.\nProceed with edit?",
                                              QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes) {
                isPostageLocked = false;
                QList<QLineEdit*> postageLineEdits = {
                    ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
                    ui->inactivePOPostage, ui->inactivePUPostage,
                    ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
                };
                for (QLineEdit *edit : postageLineEdits) {
                    edit->setReadOnly(false);
                }
                isRunPreProofComplete = false;
                updateLEDs();
                logToTerminal("Postage Data Unlocked");
            } else {
                ui->postageLock->setChecked(true);
            }
        } else {
            isPostageLocked = false;
            QList<QLineEdit*> postageLineEdits = {
                ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
                ui->inactivePOPostage, ui->inactivePUPostage,
                ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
            };
            for (QLineEdit *edit : postageLineEdits) {
                edit->setReadOnly(false);
            }
            logToTerminal("Postage Data Unlocked");
        }
    }
}

// Slot for "ALL" Checkbox State Change
void Goji::onAllCBStateChanged(int state)
{
    bool checked = (state == Qt::Checked);
    ui->cbcCB->setChecked(checked);
    ui->excCB->setChecked(checked);
    ui->inactiveCB->setChecked(checked);
    ui->ncwoCB->setChecked(checked);
    ui->prepifCB->setChecked(checked);
    if (checked) {
        ui->proofApprovalLED->setStyleSheet("background-color: #00ff15;");
    } else {
        ui->proofApprovalLED->setStyleSheet("background-color: red;");
    }
}

// Slot to Update "ALL" Checkbox Based on Individual Checkbox States
void Goji::updateAllCBState()
{
    bool allChecked = ui->cbcCB->isChecked() &&
                      ui->excCB->isChecked() &&
                      ui->inactiveCB->isChecked() &&
                      ui->ncwoCB->isChecked() &&
                      ui->prepifCB->isChecked();
    QSignalBlocker blocker(ui->allCB);
    ui->allCB->setChecked(allChecked);
    if (allChecked) {
        ui->proofApprovalLED->setStyleSheet("background-color: #00ff15;");
    } else {
        ui->proofApprovalLED->setStyleSheet("background-color: red;");
    }
}

// ComboBox Handler Implementations
void Goji::onProofDDboxChanged(const QString &text)
{
    logToTerminal("Proof dropdown changed to: " + text);
}

void Goji::onPrintDDboxChanged(const QString &text)
{
    logToTerminal("Print dropdown changed to: " + text);
}

void Goji::onYearDDboxChanged(const QString &text)
{
    logToTerminal("Year changed to: " + text);
}

void Goji::onMonthDDboxChanged(const QString &text)
{
    logToTerminal("Month changed to: " + text);
    ui->weekDDbox->clear();
    ui->weekDDbox->addItem("");
    if (!text.isEmpty()) {
        int month = text.toInt();
        int year = ui->yearDDbox->currentText().toInt();
        if (year > 0 && month > 0) {
            QDate firstDay(year, month, 1);
            QDate lastDay = firstDay.addMonths(1).addDays(-1);
            QDate date = firstDay;
            while (date <= lastDay) {
                if (date.dayOfWeek() == Qt::Monday) {
                    ui->weekDDbox->addItem(QString::number(date.day()).rightJustified(2, '0'));
                }
                date = date.addDays(1);
            }
        }
    }
}

void Goji::onWeekDDboxChanged(const QString &text)
{
    logToTerminal("Week changed to: " + text);
}

// Helper function to save post-proof counts to database
void Goji::savePostProofCounts()
{
    QSqlQuery query;

    // Drop existing tables to overwrite previous data
    query.exec("DROP TABLE IF EXISTS post_proof_counts");
    query.exec("DROP TABLE IF EXISTS count_comparison");

    // Create post_proof_counts table
    if (!query.exec("CREATE TABLE post_proof_counts ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "job_number TEXT, "
                    "week TEXT, "
                    "project TEXT, "
                    "pr_count INTEGER, "
                    "canc_count INTEGER, "
                    "us_count INTEGER, "
                    "postage REAL)")) {
        qDebug() << "Error creating post_proof_counts table:" << query.lastError().text();
        return;
    }

    // Create count_comparison table
    if (!query.exec("CREATE TABLE count_comparison ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "group_name TEXT, "
                    "input_count INTEGER, "
                    "output_count INTEGER, "
                    "difference INTEGER)")) {
        qDebug() << "Error creating count_comparison table:" << query.lastError().text();
        return;
    }

    // Placeholder for script output parsing or direct data insertion
    // This assumes 04POSTPROOF.py outputs data in a format we can parse or we fetch it directly
    // For now, we'll leave this as a placeholder to be completed with script integration
    logToTerminal("Post-proof counts saved to database (placeholder implementation).");
}

// Slot for actionGet_Count_Table
void Goji::onGetCountTableClicked()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Post-Proof Counts and Comparison");
    QVBoxLayout *layout = new QVBoxLayout(dialog);

    // Counts Table
    QTableWidget *countsTable = new QTableWidget(this);
    countsTable->setColumnCount(7);
    countsTable->setHorizontalHeaderLabels({"Job Number", "Week", "Project", "PR Count", "CANC Count", "US Count", "Postage"});
    countsTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");

    QSqlQuery countsQuery("SELECT job_number, week, project, pr_count, canc_count, us_count, postage FROM post_proof_counts");
    int row = 0;
    countsTable->setRowCount(countsQuery.size());
    while (countsQuery.next()) {
        countsTable->setItem(row, 0, new QTableWidgetItem(countsQuery.value("job_number").toString()));
        countsTable->setItem(row, 1, new QTableWidgetItem(countsQuery.value("week").toString()));
        countsTable->setItem(row, 2, new QTableWidgetItem(countsQuery.value("project").toString()));
        countsTable->setItem(row, 3, new QTableWidgetItem(countsQuery.value("pr_count").toString()));
        countsTable->setItem(row, 4, new QTableWidgetItem(countsQuery.value("canc_count").toString()));
        countsTable->setItem(row, 5, new QTableWidgetItem(countsQuery.value("us_count").toString()));
        countsTable->setItem(row, 6, new QTableWidgetItem(countsQuery.value("postage").toString()));
        row++;
    }

    QPushButton *copyCountsButton = new QPushButton("Copy Counts", dialog);
    connect(copyCountsButton, &QPushButton::clicked, this, [countsTable]() {
        QString html = "<table border='1'>";
        for (int i = 0; i < countsTable->rowCount(); ++i) {
            html += "<tr>";
            for (int j = 0; j < countsTable->columnCount(); ++j) {
                html += "<td>" + (countsTable->item(i, j) ? countsTable->item(i, j)->text() : "") + "</td>";
            }
            html += "</tr>";
        }
        html += "</table>";
        QApplication::clipboard()->setText(html, QClipboard::Clipboard);
    });

    layout->addWidget(copyCountsButton);
    layout->addWidget(countsTable);

    // Comparison Table
    QTableWidget *comparisonTable = new QTableWidget(this);
    comparisonTable->setColumnCount(4);
    comparisonTable->setHorizontalHeaderLabels({"Group", "Input Count", "Output Count", "Difference"});
    comparisonTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");

    QSqlQuery comparisonQuery("SELECT group_name, input_count, output_count, difference FROM count_comparison");
    row = 0;
    comparisonTable->setRowCount(comparisonQuery.size());
    while (comparisonQuery.next()) {
        comparisonTable->setItem(row, 0, new QTableWidgetItem(comparisonQuery.value("group_name").toString()));
        comparisonTable->setItem(row, 1, new QTableWidgetItem(comparisonQuery.value("input_count").toString()));
        comparisonTable->setItem(row, 2, new QTableWidgetItem(comparisonQuery.value("output_count").toString()));
        comparisonTable->setItem(row, 3, new QTableWidgetItem(comparisonQuery.value("difference").toString()));
        row++;
    }

    QPushButton *copyComparisonButton = new QPushButton("Copy Comparison", dialog);
    connect(copyComparisonButton, &QPushButton::clicked, this, [comparisonTable]() {
        QString html = "<table border='1'>";
        for (int i = 0; i < comparisonTable->rowCount(); ++i) {
            html += "<tr>";
            for (int j = 0; j < comparisonTable->columnCount(); ++j) {
                html += "<td>" + (comparisonTable->item(i, j) ? comparisonTable->item(i, j)->text() : "") + "</td>";
            }
            html += "</tr>";
        }
        html += "</table>";
        QApplication::clipboard()->setText(html, QClipboard::Clipboard);
    });

    layout->addWidget(copyComparisonButton);
    layout->addWidget(comparisonTable);

    dialog->setLayout(layout);
    dialog->resize(600, 400);
    dialog->exec();
}
