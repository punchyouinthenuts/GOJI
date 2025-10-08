#ifndef THREADUTILS_H
#define THREADUTILS_H
#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QtConcurrentMap>

#include <functional>

namespace ThreadUtils {

/**
 * @brief Runs a task asynchronously without blocking the UI thread
 * @param task The function to run asynchronously
 * @param onFinished Optional callback to execute when the task completes
 * @param onError Optional callback to execute if the task throws an exception
 * @return A QFutureWatcher object that will monitor the task
 */
template<typename Function>
QFutureWatcher<decltype(std::declval<Function>()())>* runAsync(
    Function task,
    std::function<void(const decltype(std::declval<Function>()())&)> onFinished = nullptr,
    std::function<void(const QString&)> onError = nullptr)
{
    // Create future watcher with appropriate type
    using ResultType = decltype(std::declval<Function>()());
    auto* watcher = new QFutureWatcher<ResultType>();

    // Connect signals
    if (onFinished) {
        QObject::connect(watcher, &QFutureWatcher<ResultType>::finished, [watcher, onFinished]() {
            onFinished(watcher->result());
            watcher->deleteLater();
        });
    } else {
        QObject::connect(watcher, &QFutureWatcher<ResultType>::finished, [watcher]() {
            watcher->deleteLater();
        });
    }

    // Start the task
    try {
        QFuture<ResultType> future = QtConcurrent::run(task);
        watcher->setFuture(future);
    }
    catch (const std::exception& e) {
        if (onError) {
            onError(QString("Exception starting async task: %1").arg(e.what()));
        }
        watcher->deleteLater();
        return nullptr;
    }

    return watcher;
}

/**
 * @brief Runs a void task asynchronously without blocking the UI thread
 * @param task The function to run asynchronously
 * @param onFinished Optional callback to execute when the task completes
 * @param onError Optional callback to execute if the task throws an exception
 * @return A QFutureWatcher object that will monitor the task
 */
inline QFutureWatcher<void>* runAsyncVoid(
    std::function<void()> task,
    std::function<void()> onFinished = nullptr,
    std::function<void(const QString&)> onError = nullptr)
{
    // Create future watcher for void functions
    auto* watcher = new QFutureWatcher<void>();

    // Connect signals
    if (onFinished) {
        QObject::connect(watcher, &QFutureWatcher<void>::finished, [watcher, onFinished]() {
            onFinished();
            watcher->deleteLater();
        });
    } else {
        QObject::connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);
    }

    // Start the task
    try {
        QFuture<void> future = QtConcurrent::run(task);
        watcher->setFuture(future);
    }
    catch (const std::exception& e) {
        if (onError) {
            onError(QString("Exception starting async task: %1").arg(e.what()));
        }
        watcher->deleteLater();
        return nullptr;
    }

    return watcher;
}

/**
 * @brief Runs a task on multiple data items in parallel
 * @param items The collection of items to process
 * @param task The function to apply to each item
 * @param onFinished Optional callback to execute when all items are processed
 * @param onError Optional callback to execute if processing throws an exception
 * @return A QFutureWatcher object that will monitor the task
 */
template<typename Container, typename Function>
auto runMapped(
    const Container& items,
    Function task,
    std::function<void(const QList<decltype(std::declval<Function>()(std::declval<typename Container::value_type>()))>&)> onFinished = nullptr,
    std::function<void(const QString&)> onError = nullptr)
{
    // Determine result type based on function and container item type
    using ItemType = typename Container::value_type;
    using ResultType = decltype(std::declval<Function>()(std::declval<ItemType>()));

    // Create future watcher
    auto* watcher = new QFutureWatcher<ResultType>();

    // Connect signals
    if (onFinished) {
        QObject::connect(watcher, &QFutureWatcher<ResultType>::finished, [watcher, onFinished]() {
            QList<ResultType> results;
            for (int i = 0; i < watcher->future().resultCount(); ++i) {
                results.append(watcher->future().resultAt(i));
            }
            onFinished(results);
            watcher->deleteLater();
        });
    } else {
        QObject::connect(watcher, &QFutureWatcher<ResultType>::finished, watcher, &QObject::deleteLater);
    }

    // Start the mapped task
    try {
        QFuture<ResultType> future = QtConcurrent::mapped(items, task);
        watcher->setFuture(future);
    }
    catch (const std::exception& e) {
        if (onError) {
            onError(QString("Exception starting parallel task: %1").arg(e.what()));
        }
        watcher->deleteLater();
        return nullptr;
    }

    return watcher;
}

} // namespace ThreadUtils

#endif // THREADUTILS_H
