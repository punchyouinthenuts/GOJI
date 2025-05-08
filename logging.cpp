#include "logging.h"

// Implementation simply forwards to logger.h implementation
void logMessage(const QString& message) {
    Logger::instance().info(message);
}
