#include "logging.h"
#include "logger.h"

void logMessage(const QString& message) {
    Logger::instance().info(message);
}
