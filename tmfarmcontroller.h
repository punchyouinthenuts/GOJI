#ifndef TMFARMCONTROLLER_H
#define TMFARMCONTROLLER_H

#include <QObject>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTableView>
#include <QSqlTableModel>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractButton>
#include <QProcess> // REQUIRED for QProcess::ExitStatus
#include <memory>

class ScriptRunner;
class TMFarmEmailDialog;

class TMFarmController : public QObject
{
    Q_OBJECT
public:
    explicit TMFarmController(QObject *parent = nullptr);
    ~TMFarmController();

    void setTextBrowser(QTextBrowser *browser);

    // MainWindow wires all these for FARMWORKERS (confirmed)
    void initializeUI(
        QAbstractButton *openBulkMailerBtn,
        QAbstractButton *runInitialBtn,
        QAbstractButton *finalStepBtn,
        QAbstractButton *lockButton,
        QAbstractButton *editButton,
        QAbstractButton *postageLockButton,
        QComboBox  *yearDD,
        QComboBox  *quarterDD,
        QLineEdit  *jobNumberBox,
        QLineEdit  *postageBox,
        QLineEdit  *countBox,
        QTextEdit  *terminalWindow,
        QTableView *trackerView,
        QTextBrowser *textBrowser
    );

    void refreshTracker(const QString &jobNumber);

private slots:
    // Final step (entry point)
    void onFinalStepClicked();
    void triggerArchivePhase(); // legacy alias -> runArchivePhase()

    // Other buttons
    void onRunInitialClicked();
    void onOpenBulkMailerClicked();

    // ScriptRunner (prearchive)
    void onScriptOutput(const QString& line);
    void onScriptError(const QString& line);
    void onScriptFinished(int exitCode, QProcess::ExitStatus status);

    // Archive (fresh QProcess)
    void runArchivePhase();
    void onArchiveFinished(int exitCode, QProcess::ExitStatus status);

    // Formatting slots
    void onPostageEditingFinished();
    void onCountEditingFinished();

    // HTML refresh signals
    void updateHtmlDisplay();

private:
    // Tracker setup helpers
    void setupTrackerModel();
    void setupOptimizedTableLayout();
    void applyHeaderLabels();
    void enforceVisibilityMask();
    void applyFixedColumnWidths();
    int  computeOptimalFontSize() const;

    // Widget behavior (mirrors TERM)
    void initYearDropdown();         // ["", prev, current, next]
    void setupTextBrowserInitial();  // ensure default loads initially
    void wireFormattingForInputs();  // connect editingFinished
    void formatPostageBoxDisplay();  // $ + thousands + 2 decimals
    void formatCountBoxDisplay();    // thousands

    // Dynamic HTML rule (mirrors TERM)
    int  determineHtmlState() const; // 0=default, 1=instructions
    void loadHtmlFile(const QString& resourcePath);

    // Script/flow helpers
    void parseScriptOutputLine(const QString& line);
    void updateControlStates();

private:
    // UI (non-owning)
    QTableView *m_trackerView = nullptr;
    QTextBrowser *m_textBrowser = nullptr;

    QAbstractButton *m_openBulkMailerBtn = nullptr;
    QAbstractButton *m_runInitialBtn = nullptr;
    QAbstractButton *m_finalStepBtn = nullptr;
    QAbstractButton *m_lockButton = nullptr;
    QAbstractButton *m_editButton = nullptr;
    QAbstractButton *m_postageLockButton = nullptr;

    QComboBox   *m_yearDD = nullptr;
    QComboBox   *m_quarterDD = nullptr;

    QLineEdit   *m_jobNumberBox = nullptr;
    QLineEdit   *m_postageBox = nullptr;
    QLineEdit   *m_countBox = nullptr;

    QTextEdit   *m_terminalWindow = nullptr;

    // ScriptRunner
    ScriptRunner* m_scriptRunner = nullptr;
    QString m_lastExecutedScript;
    QString m_capturedNASPath;
    bool    m_capturingNASPath = false;

    // Model
    std::unique_ptr<QSqlTableModel> m_trackerModel;
};

#endif // TMFARMCONTROLLER_H
