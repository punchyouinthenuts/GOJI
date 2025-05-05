#ifndef COUNTSTABLEDIALOG_H
#define COUNTSTABLEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include "databasemanager.h"

class QGraphicsColorizeEffect;

class CountsTableDialog : public QDialog
{
    Q_OBJECT

public:
    CountsTableDialog(DatabaseManager* dbManager, QWidget* parent = nullptr);

private slots:
    void onCopyCountsButtonClicked();

private:
    DatabaseManager* m_dbManager;
    QTableWidget* m_countsTable;
    QTableWidget* m_comparisonTable;
    QPushButton* m_copyCountsButton;
    QGraphicsColorizeEffect* m_colorEffect;

    void setupUI();
    void loadData();
};

#endif // COUNTSTABLEDIALOG_H
