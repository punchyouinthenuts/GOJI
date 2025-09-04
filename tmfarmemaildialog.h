#ifndef TMFARMEMAILDIALOG_H
#define TMFARMEMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QClipboard>
#include <QCloseEvent>
#include <QApplication>
#include <QFont>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>

#include "tmhealthyemailfilelistwidget.h"

/**
 * @brief Email integration dialog for TM FARMWORKERS
 *
 * Shows the network path and only the merged CSV for drag-and-drop into email:
 *   - Prefer: {job} FARMWORKERS_MERGED.csv
 *   - Fallback: FARMWORKERS_MERGED.csv
 */
class TMFarmEmailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TMFarmEmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent = nullptr);
    ~TMFarmEmailDialog();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCopyPathClicked();
    void onFileClicked();
    void onCloseClicked();

private:
    void setupUI();
    void populateFileList();
    void updateCloseButtonState();
    QString getFileDirectory();

    // UI
    QVBoxLayout* m_mainLayout;
    QLabel* m_headerLabel1;
    QLabel* m_headerLabel2;
    QLabel* m_pathLabel;
    QPushButton* m_copyPathButton;
    TMHealthyEmailFileListWidget* m_fileList;
    QPushButton* m_closeButton;

    // State
    QString m_networkPath;
    QString m_jobNumber;
    bool m_copyClicked;
    bool m_fileClicked;

    // Icons
    QFileIconProvider m_iconProvider;

    // Constants
    static const QString DATA_DIR;     // "C:/Goji/TRACHMAR/FARMWORKERS/DATA"
    static const QString FONT_FAMILY;  // "Blender Pro"
};

#endif // TMFARMEMAILDIALOG_H
