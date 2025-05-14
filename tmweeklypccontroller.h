#ifndef TMWEEKLYPCCONTROLLER_H
#define TMWEEKLYPCCONTROLLER_H

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTableView>
#include "databasemanager.h"
#include "tmweeklypcdbmanager.h"
#include "scriptrunner.h"
#include "tmweeklypcfilemanager.h"

class TMWeeklyPCController : public QObject
{
    Q_OBJECT

public:
    explicit TMWeeklyPCController(QObject *parent = nullptr);
    ~TMWeeklyPCController();

    // Initialize with UI elements from mainwindow
    void initializeUI(
        QPushButton* runInitialBtn, QPushButton* openBulkMailerBtn,
        QPushButton* runProofDataBtn, QPushButton* openProofFilesBtn,
        QPushButton* runWeeklyMergedBtn, QPushButton* openPrintFilesBtn,
        QPushButton* runPostPrintBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* proofDDbox, QComboBox* printDDbox,
        QComboBox* yearDDbox, QComboBox* monthDDbox, QComboBox* weekDDbox,
        QComboBox* classDDbox, QComboBox* permitDDbox, QLineEdit* jobNumberBox,
        QLineEdit* postageBox, QLineEdit* countBox, QTextEdit* terminalWindow,
        QTableView* tracker
        );

    // Load saved data
    bool loadJob(const QString& year, const QString& month, const QString& week);

private slots:
    // Button handlers
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();
    void onRunInitialClicked();
    void onOpenBulkMailerClicked();
    void onRunProofDataClicked();
    void onOpenProofFilesClicked();
    void onRunWeeklyMergedClicked();
    void onOpenPrintFilesClicked();
    void onRunPostPrintClicked();

    // Dropdown handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);
    void onClassChanged(const QString& mailClass);

    // Table context menu
    void showTableContextMenu(const QPoint& pos);

    // Script signals
    void onScriptStarted();
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // UI element pointers
    QPushButton* m_runInitialBtn;
    QPushButton* m_openBulkMailerBtn;
    QPushButton* m_runProofDataBtn;
    QPushButton* m_openProofFilesBtn;
    QPushButton* m_runWeeklyMergedBtn;
    QPushButton* m_openPrintFilesBtn;
    QPushButton* m_runPostPrintBtn;
    QToolButton* m_lockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QComboBox* m_proofDDbox;
    QComboBox* m_printDDbox;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QComboBox* m_weekDDbox;
    QComboBox* m_classDDbox;
    QComboBox* m_permitDDbox;
    QLineEdit* m_jobNumberBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextEdit* m_terminalWindow;
    QTableView* m_tracker;

    // Support objects
    DatabaseManager* m_dbManager;
    TMWeeklyPCDBManager* m_tmWeeklyPCDBManager;
    ScriptRunner* m_scriptRunner;
    QSqlTableModel* m_trackerModel;
    TMWeeklyPCFileManager* m_fileManager;

    // State variables
    bool m_jobDataLocked;
    bool m_postageDataLocked;

    // Utility methods
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void populateWeekDDbox();
    void formatPostageInput();
    bool validateJobData();
    bool validatePostageData();
    void updateControlStates();
    void outputToTerminal(const QString& message);
    void createBaseDirectories();
    void createJobFolder();
    void saveJobToDatabase();
    void addLogEntry();
    QString copyFormattedRow();
};

#endif // TMWEEKLYPCCONTROLLER_H
