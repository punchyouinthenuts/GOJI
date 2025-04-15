#ifndef GOJI_H
#define GOJI_H

// Updated Qt 6 includes with module prefixes
#include <QtWidgets/QMainWindow>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>
#include <functional>
#include <QtCore/QMap>
#include <QtCore/QFile>
#include <QtWidgets/QMessageBox>
#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtWidgets/QCheckBox>
#include <QtGui/QRegularExpressionValidator>
#include <QtCore/QPair>
#include <QtWidgets/QMenu>
#include <array> // For std::array

#include "ui_GOJI.h" // Generated UI header

class Goji : public QMainWindow
{
    Q_OBJECT

public:
    /** Constructor: Initializes the main window */
    explicit Goji(QWidget *parent = nullptr);

    /** Destructor: Cleans up resources */
    ~Goji();

private slots:
    // **Button Handlers for QPushButton**
    void onOpenIZClicked();          // Opens InDesign files
    void onRunInitialClicked();      // Runs initial processing script
    void onRunPreProofClicked();     // Runs pre-proof processing
    void onOpenProofFilesClicked();  // Opens proof files
    void onRunPostProofClicked();    // Runs post-proof processing
    void onOpenPrintFilesClicked();  // Opens print files
    void onRunPostPrintClicked();    // Runs post-print processing

    // **Button Handlers for QToolButton**
    void onLockButtonToggled(bool checked);      // Toggles job data lock
    void onEditButtonToggled(bool checked);      // Toggles edit mode
    void onProofRegenToggled(bool checked);      // Toggles proof regeneration mode
    void onPostageLockToggled(bool checked);     // Toggles postage field lock

    // **ComboBox Handlers**
    void onProofDDboxChanged(const QString &text);   // Updates proof dropdown selection
    void onPrintDDboxChanged(const QString &text);   // Updates print dropdown selection
    void onYearDDboxChanged(const QString &text);    // Updates year dropdown selection
    void onMonthDDboxChanged(const QString &text);   // Updates month dropdown selection
    void onWeekDDboxChanged(const QString &text);    // Updates week dropdown selection

    // **Checkbox Handlers**
    void onAllCBStateChanged(Qt::CheckState state);  // Handles "All" checkbox state change
    void updateAllCBState(Qt::CheckState state);     // Updates "All" checkbox state

    // **File and Script Handling Slots**
    void onPrintDirChanged(const QString &path);     // Monitors print directory changes
    void checkPrintFiles(const QString& selection);  // Checks print file status
    void checkProofFiles(const QString& selection);  // Checks proof file status

    // **Utility Slots**
    void formatCurrencyOnFinish();                   // Formats currency fields on completion
    void onGetCountTableClicked();                   // Retrieves count table data
    void onRegenProofButtonClicked();                // Regenerates proof files

    // **Menu Action Slots**
    void onActionExitTriggered();                    // Exits the application
    void onActionCloseJobTriggered();                // Closes the current job
    void onActionSaveJobTriggered();                 // Saves the current job

    // **Dynamic Menu Slots**
    void buildWeeklyMenu();                          // Builds the Weekly menu dynamically
    void openJobFromWeekly(int year, int month, int week);  // Opens job by year/month/week (int)
    void openJobFromWeekly(const QString& year, const QString& month, const QString& week);  // Opens job by year/month/week (string)
    void onCheckForUpdatesTriggered();               // Checks for application updates

private:
    Ui::MainWindow *ui;                  // Pointer to the UI object
    QMenu *openJobMenu;                  // Submenu for opening jobs
    QMenu *weeklyMenu;                   // Submenu for weekly jobs

    // **File Monitoring Members**
    QStringList m_printDirs;             // List of print directories
    QFileSystemWatcher *m_printWatcher;  // Watches print directories for changes
    QTimer *m_inactivityTimer;           // Timer for inactivity detection

    // **File Mappings**
    QMap<QString, QStringList> proofFiles;         // Maps job types to proof files
    QMap<QString, QStringList> printFiles;         // Maps job types to print files
    QMap<QString, QCheckBox*> regenCheckboxes;     // Maps job types to regen checkboxes
    QMap<QCheckBox*, QPair<QString, QString>> checkboxFileMap;  // Maps checkboxes to files

    // **Database and State Management**
    QSqlDatabase db;                     // Database connection
    bool isJobSaved;                     // Tracks if the job is saved
    QString originalYear;                // Original year of the job
    QString originalMonth;               // Original month of the job
    QString originalWeek;                // Original week of the job
    bool isPostageLocked;                // Tracks if postage fields are locked
    bool isProofRegenMode;               // Tracks proof regeneration mode
    QRegularExpressionValidator *validator;  // Validates input fields

    // **Process Completion Flags**
    bool isOpenIZComplete;               // Flag for Open IZ completion
    bool isRunInitialComplete;           // Flag for initial run completion
    bool isRunPreProofComplete;          // Flag for pre-proof run completion
    bool isOpenProofFilesComplete;       // Flag for proof files opened
    bool isRunPostProofComplete;         // Flag for post-proof run completion
    bool isOpenPrintFilesComplete;       // Flag for print files opened
    bool isRunPostPrintComplete;         // Flag for post-print run completion

    // **Weekly Progress Bar Tracking**
    static constexpr size_t NUM_STEPS = 9;          // Number of progress steps
    std::array<double, NUM_STEPS> stepWeights;      // Weights for each step
    std::array<int, NUM_STEPS> totalSubtasks;       // Total subtasks per step
    std::array<int, NUM_STEPS> completedSubtasks;   // Completed subtasks per step

    // **State Management**
    bool isJobDataLocked;                // Tracks if job data fields are locked

    // **Helper Methods**
    void logToTerminal(const QString &message);      // Logs messages to terminal
    void clearJobNumbers();                          // Clears job number fields
    QString getProofFolderPath(const QString &jobType);  // Gets proof folder path
    void runScript(const QString &program, const QStringList &arguments);  // Runs external script
    void ensureInDesignIsOpen(const std::function<void()>& callback);  // Ensures InDesign is open
    void openProofFiles(const QString& selection);   // Opens proof files for selection
    void openPrintFiles(const QString& selection);   // Opens print files for selection
    void lockJobDataFields(bool lock);               // Locks/unlocks job data fields
    void lockPostageFields(bool lock);               // Locks/unlocks postage fields
    void updateLEDs();                               // Updates LED indicators
    void updateButtonStates(bool enabled);           // Updates button states
    int getNextProofVersion(const QString& filePath);  // Gets next proof version number
    void regenerateProofs();                         // Regenerates proof files
    void enableProofApprovalCheckboxes();            // Enables proof approval checkboxes
    void updateWidgetStatesBasedOnJobState();        // Updates widgets based on job state
    void updateProgressBar();                        // Updates the progress bar

    // **Database Helper Methods**
    bool jobExists(const QString& year, const QString& month, const QString& week);  // Checks if job exists
    void insertJob();                                // Inserts a new job into the database
    void updateJob();                                // Updates an existing job
    void deleteJob(const QString& year, const QString& month, const QString& week);  // Deletes a job

    // **Additional Helper Methods**
    void savePostProofCounts();                      // Saves post-proof counts
    void runProofRegenScript(const QString& jobType, const QStringList& files, int version);  // Runs proof regen script
    QString getJobNumberForJobType(const QString& jobType);  // Gets job number for job type
    void copyFilesFromHomeToWorking(const QString& year, const QString& month, const QString& week);  // Copies files to working dir

    // **Folder and File Management Helpers**
    void createJobFolders(const QString& year, const QString& month, const QString& week);  // Creates job folders
    void copyFilesToWorkingFolders(const QString& year, const QString& month, const QString& week);  // Copies files to working folders
    void moveFilesToHomeFolders(const QString& year, const QString& month, const QString& week);  // Moves files to home folders
};

#endif // GOJI_H
