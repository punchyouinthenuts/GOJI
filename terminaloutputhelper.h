#ifndef TERMINALOUTPUTHELPER_H
#define TERMINALOUTPUTHELPER_H

#include <QDateTime>
#include <QString>

class QTextEdit;

enum class TerminalSeverity {
    Info,
    Success,
    Warning,
    Error
};

class TerminalOutputHelper
{
public:
    static QString normalizeMessage(const QString& message, TerminalSeverity severity);
    static QString formatHtmlLine(const QString& message,
                                  TerminalSeverity severity,
                                  const QDateTime& now = QDateTime::currentDateTime());
    static void append(QTextEdit* terminal,
                       const QString& message,
                       TerminalSeverity severity = TerminalSeverity::Info);

private:
    static QString severityPrefix(TerminalSeverity severity);
    static QString severityClass(TerminalSeverity severity);
};

#endif // TERMINALOUTPUTHELPER_H
