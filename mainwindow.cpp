void MainWindow::formatCurrencyOnFinish()
{
    // Format currency values in line edits
    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) return;
    
    QString text = lineEdit->text();
    if (text.isEmpty()) return;
    
    // Parse value and format with 2 decimal places
    bool ok;
    double value = text.toDouble(&ok);
    if (ok) {
        lineEdit->setText(QString::number(value, 'f', 2));
    }
}

void MainWindow::onPrintDirChanged(const QString& path)
{
    // Handle print directory changes
    logMessage("Print directory changed: " + path);
    
    // Scan for new files and update UI as needed
    QDir dir(path);
    QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    
    logToTerminal(tr("Print directory updated. Found %1 files.").arg(entries.count()));
    
    // Additional processing if needed
}

void MainWindow::onInactivityTimeout()
{
    // Handle inactivity timer timeout
    logMessage("Inactivity timeout triggered.");
    
    // Auto-save or other periodic actions
    if (m_jobController && m_jobController->isJobSaved()) {
        // Consider auto-saving job state
        m_jobController->saveJob();
        logMessage("Auto-saved job state due to inactivity.");
    }
}

void MainWindow::onScriptStarted(const QString& scriptType)
{
    // Handle script start event
    logMessage("Script started: " + scriptType);
    logToTerminal(tr("Running %1 script...").arg(scriptType));
    
    // Update UI for script running state
    ui->progressBarWeekly->setValue(0);
    // Consider disabling relevant buttons during script execution
}

void MainWindow::onScriptFinished(const QString& scriptType, bool success)
{
    // Handle script finished event
    logMessage("Script finished: " + scriptType + ", success: " + QString(success ? "true" : "false"));
    
    if (success) {
        logToTerminal(tr("%1 script completed successfully.").arg(scriptType));
        ui->progressBarWeekly->setValue(100);
    } else {
        logToTerminal(tr("%1 script failed.").arg(scriptType));
        ui->progressBarWeekly->setValue(0);
    }
    
    // Update UI based on script result
    updateLEDs();
    updateWidgetStatesBasedOnJobState();
    updateBugNudgeMenu();
}

void MainWindow::onJobProgressUpdated(int percentage)
{
    // Handle job progress updates
    ui->progressBarWeekly->setValue(percentage);
}

void MainWindow::onLogMessage(const QString& message)
{
    // Handle log messages from other components
    logMessage(message);
}

void MainWindow::logMessage(const QString& message)
{
    // Central log message handler
    // Use QMutexLocker for thread safety if needed
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logEntry = timestamp + " - " + message + "\n";
    
    // Write to log file
    if (logFile.isOpen()) {
        logFile.write(logEntry.toUtf8());
        logFile.flush();
    }
    
    // Also output to console in debug mode
#ifdef QT_DEBUG
    qDebug() << message;
#endif
}

void MainWindow::logToTerminal(const QString& message)
{
    // Log message to the terminal/output window in the UI
    if (!ui || !ui->terminalOutput) return;
    
    // Add timestamp
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMessage = "[" + timestamp + "] " + message;
    
    // Append to terminal
    ui->terminalOutput->appendPlainText(formattedMessage);
    
    // Auto-scroll to bottom
    QTextCursor cursor = ui->terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->terminalOutput->setTextCursor(cursor);
    
    // Also log to file
    logMessage("TERMINAL: " + message);
}

void MainWindow::buildWeeklyMenu()
{
    // Populate the weekly menu dynamically
    if (!weeklyMenu) return;
    
    weeklyMenu->clear();
    
    // Get list of weekly jobs from database
    QList<JobSummary> jobs = m_dbManager->getRecentJobs(10);
    
    if (jobs.isEmpty()) {
        QAction* noJobsAction = new QAction(tr("No recent jobs"), this);
        noJobsAction->setEnabled(false);
        weeklyMenu->addAction(noJobsAction);
    } else {
        for (const JobSummary& job : jobs) {
            QString jobTitle = job.year + "-" + job.month + "-" + job.week;
            QAction* jobAction = new QAction(jobTitle, this);
            
            // Use a lambda to capture the job ID
            connect(jobAction, &QAction::triggered, this, [this, jobId = job.id]() {
                m_jobController->loadJob(jobId);
                updateLEDs();
                updateWidgetStatesBasedOnJobState();
                updateBugNudgeMenu();
                updateInstructions();
                logToTerminal(tr("Loaded job: %1").arg(jobId));
            });
            
            weeklyMenu->addAction(jobAction);
        }
    }
}

void MainWindow::populateScriptMenu(QMenu* menu, const QString& scriptDir)
{
    // Populate a script menu with script files from directory
    if (!menu) return;
    
    QDir dir(scriptDir);
    if (!dir.exists()) {
        QAction* errorAction = new QAction(tr("Directory not found"), this);
        errorAction->setEnabled(false);
        menu->addAction(errorAction);
        return;
    }
    
    QStringList scriptFiles = dir.entryList(QStringList() << "*.bat" << "*.cmd" << "*.ps1", QDir::Files);
    
    if (scriptFiles.isEmpty()) {
        QAction* noScriptsAction = new QAction(tr("No scripts found"), this);
        noScriptsAction->setEnabled(false);
        menu->addAction(noScriptsAction);
    } else {
        for (const QString& scriptFile : scriptFiles) {
            QAction* scriptAction = new QAction(scriptFile, this);
            
            // Use a lambda to capture script path
            connect(scriptAction, &QAction::triggered, this, [this, scriptPath = dir.absoluteFilePath(scriptFile)]() {
                // Run script
                m_scriptRunner->runScript(scriptPath);
                logToTerminal(tr("Running script: %1").arg(scriptPath));
            });
            
            menu->addAction(scriptAction);
        }
    }
}
