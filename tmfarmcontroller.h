#ifndef TMFARMCONTROLLER_H
#define TMFARMCONTROLLER_H

#include <QObject>
#include <QTableView>
#include <QSqlTableModel>
#include <QTextBrowser>
#include <QTextEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractButton>
#include <memory>

class TMFarmController : public QObject
{
    Q_OBJECT
public:
    explicit TMFarmController(QObject *parent = nullptr);
    ~TMFarmController();

    // API
    void setTextBrowser(QTextBrowser *browser);

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

private:
    void setupTrackerModel();
    void setupOptimizedTableLayout();

private:
    // UI widgets (non-owning)
    QTableView   *m_trackerView;
    QTextBrowser *m_textBrowser;

    QAbstractButton *m_openBulkMailerBtn;
    QAbstractButton *m_runInitialBtn;
    QAbstractButton *m_finalStepBtn;
    QAbstractButton *m_lockButton;
    QAbstractButton *m_editButton;
    QAbstractButton *m_postageLockButton;

    QComboBox   *m_yearDD;
    QComboBox   *m_quarterDD;

    QLineEdit   *m_jobNumberBox;
    QLineEdit   *m_postageBox;
    QLineEdit   *m_countBox;

    QTextEdit   *m_terminalWindow;

    // Model
    std::unique_ptr<QSqlTableModel> m_trackerModel;
};

#endif // TMFARMCONTROLLER_H
