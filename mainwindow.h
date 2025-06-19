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

#include "databasemanager.h"
#include "filesystemmanager.h"
#include "scriptrunner.h"
#include "updatemanager.h"
#include "tmweeklypccontroller.h"
#include "tmweeklypidocontroller.h"
#include "tmtermcontroller.h"

// Qt namespace declaration - make sure your project is properly configured for Qt
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Menu actions
    void onActionExitTriggered();
    void onCheckForUpdatesTriggered();
    void onUpdateSettingsTriggered();
    void onUpdateMeteredRateTriggered();

    // Tab change handler
    void onTabChanged(int index);

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

    // UI components
    QMenu* openJobMenu;
    QFileSystemWatcher* m_printWatcher;
    QTimer* m_inactivityTimer;

    // Keyboard shortcuts
    QShortcut* m_saveJobShortcut;
    QShortcut* m_closeJobShortcut;
    QShortcut* m_exitShortcut;
    QShortcut* m_tabCycleShortcut;

    // State variables
    bool m_minimalMode = false;

    // Private methods
    void setupUi();
    void setupMenus();
    void setupSignalSlots();
    void setupKeyboardShortcuts();
    void initWatchersAndTimers();
    void setupPrintWatcher();  // Your new method

    void populateScriptMenu(QMenu* menu, const QString& dirPath);
    void openScriptFile(const QString& filePath);
    void logToTerminal(const QString& message);
};

#endif // MAINWINDOW_H
