#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Use standard Qt include style
#include <QMainWindow>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QRegularExpressionValidator>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>
#include <QCheckBox>
#include <QPair>
#include <QCloseEvent>
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>
#include <QShortcut>
#include <QKeySequence>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QProcess>

// Windows-specific includes for ShellExecute
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include "databasemanager.h"
#include "filesystemmanager.h"
#include "scriptrunner.h"
#include "updatemanager.h"
#include "tmtarragoncontroller.h"
#include "tmweeklypccontroller.h"
#include "tmweeklypidocontroller.h"
#include "tmtermcontroller.h"
#include "tmflercontroller.h"
#include "tmhealthycontroller.h"

// Qt namespace declaration - make sure your project is properly configured for Qt
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Custom dialog for choosing which program to open script files with
class ScriptOpenDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScriptOpenDialog(const QString& filePath, QWidget* parent = nullptr);
    QString getSelectedProgram() const;

private slots:
    void onProgramSelected();

private:
    QString m_selectedProgram;
    QString m_filePath;
    void setupUI();
    QStringList getAvailablePrograms(const QString& extension);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    // Menu actions
    void onActionExitTriggered();
    void onCheckForUpdatesTriggered();
    void onUpdateSettingsTriggered();
    void onUpdateMeteredRateTriggered();
    void onManageEditDatabaseTriggered();
    void onSaveJobTriggered();
    void onCloseJobTriggered();
    void populateOpenJobMenu();

    // Tab change handler
    void onTabChanged(int index);
    void onCustomerTabChanged(int index);

    // File system and timer slots
    void onPrintDirChanged(const QString &path);
    void onInactivityTimeout();

    // Keyboard shortcut handler
    void cycleToNextTab();

private:
    // UI and core components
    Ui::MainWindow* ui;
    QSettings* m_settings;

    // Managers and core systems
    DatabaseManager* m_dbManager;
    FileSystemManager* m_fileManager;
    ScriptRunner* m_scriptRunner;
    UpdateManager* m_updateManager;

    // Tab controllers
    TMWeeklyPCController* m_tmWeeklyPCController;
    TMWeeklyPIDOController* m_tmWeeklyPIDOController;
    TMTermController* m_tmTermController;
    TMTarragonController* m_tmTarragonController;
    TMFLERController* m_tmFlerController;
    TMHealthyController* m_tmHealthyController;

    // UI components
    QMenu* openJobMenu;
    QFileSystemWatcher* m_printWatcher;
    QTimer* m_inactivityTimer;

    // Keyboard shortcuts
    QShortcut* m_saveJobShortcut;
    QShortcut* m_closeJobShortcut;
    QShortcut* m_exitShortcut;
    QShortcut* m_tabCycleShortcut;

    // Job menu population methods
    void populateTMWPCJobMenu();
    void populateTMTermJobMenu();
    void populateTMTarragonJobMenu();
    void populateTMFLERJobMenu();
    void populateTMHealthyJobMenu();
    void loadTMWPCJob(const QString& year, const QString& month, const QString& week);
    void loadTMTermJob(const QString& year, const QString& month);
    void loadTMTarragonJob(const QString& year, const QString& month, const QString& dropNumber);
    void loadTMFLERJob(const QString& year, const QString& month);
    void loadTMHealthyJob(const QString& year, const QString& month);

    // State variables
    bool m_minimalMode = false;
    bool m_closingJob = false; // prevents double-close races

    // Private methods
    void setupUi();
    void setupMenus();
    void setupSignalSlots();
    void setupKeyboardShortcuts();
    void initWatchersAndTimers();
    void setupPrintWatcher();

    // Month conversion utility
    QString convertMonthToAbbreviation(const QString& monthNumber) const;

    void populateScriptMenu(QMenu* menu, const QString& dirPath);
    void openScriptFile(const QString& filePath);
    void logToTerminal(const QString& message);
    
    // Dynamic script menu methods
    void setupScriptsMenu();
    void buildScriptMenuRecursively(QMenu* parentMenu, const QString& dirPath, const QString& styleSheet);
    bool isScriptFile(const QString& fileName);
    QAction* createScriptFileAction(const QFileInfo& fileInfo);
    void openScriptFileWithDialog(const QString& filePath);
    void openScriptFileWithWindowsDialog(const QString& filePath);
    
    // Meter rate management
    bool setCurrentJobTab(int index);
    double getCurrentMeterRate();
    bool updateMeterRateInDatabase(double newRate);
    bool ensureMeterRatesTableExists();
    bool requestCloseCurrentJob(bool viaAppExit);
    bool hasOpenJobForCurrentTab() const;
};

#endif // MAINWINDOW_H
