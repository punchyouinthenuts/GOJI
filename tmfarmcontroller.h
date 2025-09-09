#ifndef TMFARMCONTROLLER_H
#define TMFARMCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QSqlTableModel>
#include <QRegularExpression>
#include <memory>

class QComboBox;
class QLineEdit;
class QAbstractButton;  // base for QPushButton / QToolButton
class QPushButton;
class QToolButton;
class QTableView;
class QTextEdit;
class QTextBrowser;

class TMFarmController : public QObject
{
    Q_OBJECT
public:
    explicit TMFarmController(QObject *parent = nullptr);

    void attachWidgets(QComboBox *yearDD,
                       QComboBox *quarterDD,
                       QLineEdit *jobNumberBox,
                       QLineEdit *postageBox,
                       QLineEdit *countBox,
                       QAbstractButton *lockButton,
                       QAbstractButton *editButton,
                       QAbstractButton *postageLockButton,
                       QTableView *trackerView,
                       QTextBrowser *htmlView);

    void initialize();
    void setTextBrowser(QTextBrowser *htmlView);

    void initializeUI(QComboBox *yearDD,
                      QComboBox *quarterDD,
                      QLineEdit *jobNumberBox,
                      QLineEdit *postageBox,
                      QLineEdit *countBox,
                      QPushButton *lockButton,
                      QPushButton *editButton,
                      QPushButton *postageLockButton,
                      QTableView *trackerView);

    void initializeUI(QPushButton *openBulkMailerBtn,
                      QPushButton *runInitialBtn,
                      QPushButton *finalStepBtn,
                      QToolButton *lockButton,
                      QToolButton *editButton,
                      QToolButton *postageLockButton,
                      QComboBox *yearDD,
                      QComboBox *quarterDD,
                      QLineEdit *jobNumberBox,
                      QLineEdit *postageBox,
                      QLineEdit *countBox,
                      QTextEdit *terminalWindow,
                      QTableView *trackerView,
                      QTextBrowser *htmlView);

private slots:
    void formatPostageInput();
    void formatCountInput(const QString &text);

    void onJobNumberEditingFinished();
    void onLockClicked();
    void onEditClicked();
    void onPostageLockClicked();
    void onPostageEditingFinished();
    void onCountEditingFinished();
    void refreshTracker();

private:
    void populateYearCombo();
    void setupTrackerModel();
    void setupOptimizedTableLayout();
    void connectSignals();
    void updateControlStates();
    void updateHtmlDisplay();

    void ensureTables();
    void loadJobState();
    void saveJobState();

    QString currentJobNumber() const;
    QString currentYearText() const;
    QString currentQuarterText() const;
    bool validateJobNumber(const QString &job) const;
    QString normalizePostage(const QString &raw) const;
    QString normalizeCount(const QString &raw) const;

private:
    QPointer<QComboBox> m_yearDD;
    QPointer<QComboBox> m_quarterDD;
    QPointer<QLineEdit> m_jobNumberBox;
    QPointer<QLineEdit> m_postageBox;
    QPointer<QLineEdit> m_countBox;
    QPointer<QAbstractButton> m_lockButton;
    QPointer<QAbstractButton> m_editButton;
    QPointer<QAbstractButton> m_postageLockButton;
    QPointer<QTableView> m_trackerView;
    QPointer<QTextBrowser> m_htmlView;

    QPointer<QPushButton> m_openBulkMailerBtn;
    QPointer<QPushButton> m_runInitialBtn;
    QPointer<QPushButton> m_finalStepBtn;
    QPointer<QTextEdit>   m_terminalWindow;

    std::unique_ptr<QSqlTableModel> m_trackerModel;
    bool m_jobLocked = false;
    bool m_postageLocked = false;
};

#endif // TMFARMCONTROLLER_H
