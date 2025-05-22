
#ifndef TMWEEKLYPCCONTROLLER_H
#define TMWEEKLYPCCONTROLLER_H

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTableView>
#include <QSqlTableModel>
#include "databasemanager.h"
#include "tmweeklypcdbmanager.h"
#include "scriptrunner.h"
#include "tmweeklypcfilemanager.h"

class TMWeeklyPCController : public QObject
{
    Q_OBJECT

public:
    // Message type enum for colored terminal output
    enum MessageType {
        Info,
        Warning,
        Error,
        Success
    };

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
    void onOpenProofFileClicked();
    void onRunWeeklyMergedClicked();
    void onOpenPrintFileClicked();
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
    QPushButton* m_runInitialBtn = nullptr;
    QPushButton* m_openBulkMailerBtn = nullptr;
    QPushButton* m_runProofDataBtn = nullptr;
    QPushButton* m_openProofFileBtn = nullptr;
    QPushButton* m_runWeeklyMergedBtn = nullptr;
    QPushButton* m_openPrintFileBtn = nullptr;
    QPushButton* m_runPostPrintBtn = nullptr;
    QToolButton* m_lockBtn = nullptr;
    QToolButton* m_editBtn = nullptr;
    QToolButton* m_postageLockBtn = nullptr;
    QComboBox* m_proofDDbox = nullptr;
    QComboBox* m_printDDbox = nullptr;
    QComboBox* m_yearDDbox = nullptr;
    QComboBox* m_monthDDbox = nullptr;
    QComboBox* m_weekDDbox = nullptr;
    QComboBox* m_classDDbox = nullptr;
    QComboBox* m_permitDDbox = nullptr;
    QLineEdit* m_jobNumberBox = nullptr;
    QLineEdit* m_postageBox = nullptr;
    QLineEdit* m_countBox = nullptr;
    QTextEdit* m_terminalWindow = nullptr;
    QTableView* m_tracker = nullptr;

    // Support objects
    DatabaseManager* m_dbManager = nullptr;
    TMWeeklyPCDBManager* m_tmWeeklyPCDBManager = nullptr;
    ScriptRunner* m_scriptRunner = nullptr;
    QSqlTableModel* m_trackerModel = nullptr;
    TMWeeklyPCFileManager* m_fileManager = nullptr;

    // State variables
    bool m_jobDataLocked = false;
    bool m_postageDataLocked = false;

    // Utility methods
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void populateWeekDDbox();
    void formatPostageInput();
    bool validateJobData();
    bool validatePostageData();
    void updateControlStates();
    void outputToTerminal(const QString& message, MessageType type = Info);
    void createBaseDirectories();
    void createJobFolder();
    void saveJobToDatabase();
    void addLogEntry();
    QString copyFormattedRow();
};

#endif // TMWEEKLYPCCONTROLLER_H
