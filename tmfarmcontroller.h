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
    QTableView   *m_trackerView = nullptr;
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

    std::unique_ptr<QSqlTableModel> m_trackerModel;
};

#endif // TMFARMCONTROLLER_H
