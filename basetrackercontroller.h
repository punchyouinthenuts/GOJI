#ifndef BASETRACKERCONTROLLER_H
#define BASETRACKERCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QSqlTableModel>
#include <QModelIndex>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>

/**
 * @brief Base class for all tracker controllers providing shared Excel copy functionality
 *
 * This class provides the standardized copyFormattedRow() functionality that creates
 * a temporary Excel file, formats it with borders and proper cell formatting,
 * copies to clipboard, and cleans up automatically. All tracker controllers
 * inherit this identical functionality to ensure consistency across all tabs.
 */
class BaseTrackerController : public QObject
{
    Q_OBJECT

public:
    enum MessageType {
        Info,
        Success,
        Warning,
        Error
    };

protected:
    explicit BaseTrackerController(QObject *parent = nullptr);

    /**
     * @brief Copy selected row from tracker table with Excel formatting
     * @return Status message indicating success or failure
     *
     * This method creates a temporary Excel file with proper formatting,
     * borders, and cell styling, copies the data to clipboard, then
     * automatically cleans up the temporary file.
     */
    QString copyFormattedRow();

    /**
     * @brief Create Excel file and copy formatted data to clipboard
     * @param headers Column headers for the Excel file
     * @param rowData Row data to be formatted and copied
     * @return True if operation succeeded, false otherwise
     *
     * Uses PowerShell script to create Excel COM object, format cells
     * with borders and proper data types, copy to clipboard, and cleanup.
     */
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    /**
     * @brief Pure virtual method for outputting messages to terminal
     * @param message Message to output
     * @param type Type of message (Info, Success, Warning, Error)
     *
     * Must be implemented by derived classes to provide terminal output
     * specific to their implementation.
     */
    virtual void outputToTerminal(const QString& message, MessageType type) = 0;

    /**
     * @brief Get the tracker table view widget
     * @return Pointer to QTableView widget, must be implemented by derived classes
     */
    virtual QTableView* getTrackerWidget() const = 0;

    /**
     * @brief Get the tracker model
     * @return Pointer to QSqlTableModel, must be implemented by derived classes
     */
    virtual QSqlTableModel* getTrackerModel() const = 0;

    /**
     * @brief Get column headers for the tracker table
     * @return List of column headers specific to each tracker type
     */
    virtual QStringList getTrackerHeaders() const = 0;

    /**
     * @brief Get visible column indices for copying
     * @return List of column indices that should be included in copy operation
     */
    virtual QList<int> getVisibleColumns() const = 0;

    /**
     * @brief Apply custom formatting to specific columns during copy
     * @param columnIndex Index of the column being processed
     * @param cellData Data from the cell
     * @return Formatted cell data
     */
    virtual QString formatCellData(int columnIndex, const QString& cellData) const;

private:
};

#endif // BASETRACKERCONTROLLER_H
