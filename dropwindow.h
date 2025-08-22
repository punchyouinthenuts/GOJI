#ifndef DROPWINDOW_H
#define DROPWINDOW_H

#include <QListView>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QStringList>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRect>
#include <QIcon>

/**
 * @brief Custom QListView with drag and drop functionality for file uploads
 *
 * This widget allows users to drag and drop files and automatically copies them 
 * to a specified directory. The supported file types are dynamically configured
 * via setSupportedExtensions(), and the hint text is auto-generated from the 
 * configured extensions to provide users with contextual instructions.
 *
 * The list of supported file extensions is configurable per controller using
 * setSupportedExtensions(). By default, it accepts "xlsx", "xls", and "csv" files.
 */
class DropWindow : public QListView
{
    Q_OBJECT

public:
    explicit DropWindow(QWidget* parent = nullptr);

    /**
     * @brief Set the target directory where dropped files will be copied
     * @param targetPath Full path to target directory
     */
    void setTargetDirectory(const QString& targetPath);

    /**
     * @brief Get the current target directory
     * @return Target directory path
     */
    QString getTargetDirectory() const;

    /**
     * @brief Add a file to the display list
     * @param filePath Path to the file to add
     */
    void addFile(const QString& filePath);

    /**
     * @brief Clear all files from the display
     */
    void clearFiles();

    /**
     * @brief Get list of all files currently shown
     * @return List of file paths
     */
    QStringList getFiles() const;

    /**
     * @brief Set the list of supported file extensions (e.g., "zip", "csv")
     * @param extensions List of allowed file extensions (lowercase, without dot)
     */
    void setSupportedExtensions(const QStringList& extensions);

protected:
    // Drag and drop event handlers
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    /**
     * @brief Emitted when files are successfully dropped and copied
     * @param filePaths List of file paths that were processed
     */
    void filesDropped(const QStringList& filePaths);

    /**
     * @brief Emitted when a file drop operation fails
     * @param errorMessage Description of the error
     */
    void fileDropError(const QString& errorMessage);

    /**
     * @brief Emitted when files are added to the list
     * @param fileCount Number of files currently in the list
     */
    void fileCountChanged(int fileCount);

private slots:
    void onItemDoubleClicked(const QModelIndex& index);

private:
    QString m_targetDirectory;
    QStandardItemModel* m_model;
    QStringList m_supportedExtensions;

    // Visual state
    bool m_isDragActive;

    /**
     * @brief Check if a file type is supported for processing
     * @param filePath Path to the file to check
     * @return True if file type is supported
     */
    bool isValidFileType(const QString& filePath) const;

    /**
     * @brief Copy a file to the target directory
     * @param sourcePath Source file path
     * @param targetDir Target directory
     * @return Final absolute path of copied file, empty on failure
     */
    QString copyFileToTarget(const QString& sourcePath, const QString& targetDir);

    /**
     * @brief Generate unique filename if target already exists
     * @param targetPath Original target path
     * @return Unique target path
     */
    QString generateUniqueFilename(const QString& targetPath) const;

    /**
     * @brief Update the visual display based on drag state
     */
    void updateVisualState();

    /**
     * @brief Set up the model and view
     */
    void setupModel();

    /**
     * @brief Handle ZIP file drop by virtually listing contents (no extraction/copy)
     * @param zipPath Absolute path to the .zip archive
     */
    void handleZipDrop(const QString& zipPath);

    void addVirtualZipEntry(const QString& archivePath,
                            const QString& internalPath,
                            const QString& displayName,
                            quint64 size,
                            bool isDir);

    QIcon iconForFileName(const QString& fileName); // picks an icon by extension without requiring a real file
};

#endif // DROPWINDOW_H
