#ifndef COUNTSTABLEDIALOG_H
#define COUNTSTABLEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "databasemanager.h"

class CountsTableDialog : public QDialog
{
    Q_OBJECT

public:
    CountsTableDialog(DatabaseManager* dbManager, QWidget* parent = nullptr);

private slots:
    void onCopyCountsButtonClicked();
    void onCopyComparisonButtonClicked();

private:
    DatabaseManager* m_dbManager;
    QTableWidget* m_countsTable;
    QTableWidget* m_comparisonTable;

    void setupUI();
    void loadData();
};

#endif // COUNTSTABLEDIALOG_H
