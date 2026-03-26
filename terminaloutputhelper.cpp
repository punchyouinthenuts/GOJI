#include "terminaloutputhelper.h"

#include <QPalette>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>

namespace {
QString severityToken(TerminalSeverity severity)
{
    switch (severity) {
    case TerminalSeverity::Info:
        return "info";
    case TerminalSeverity::Success:
        return "success";
    case TerminalSeverity::Warning:
        return "warning";
    case TerminalSeverity::Error:
        return "error";
    }

    return QString();
}

QString stripMatchingLeadingSeverityToken(const QString& message, TerminalSeverity severity)
{
    const QString token = severityToken(severity);
    if (token.isEmpty()) {
        return message;
    }

    QString normalized = message;

    while (true) {
        int index = 0;
        while (index < normalized.size() && normalized.at(index).isSpace()) {
            ++index;
        }

        if (normalized.mid(index, token.size()).compare(token, Qt::CaseInsensitive) != 0) {
            break;
        }

        index += token.size();
        while (index < normalized.size() && normalized.at(index).isSpace()) {
            ++index;
        }

        if (index >= normalized.size() || normalized.at(index) != QChar(':')) {
            break;
        }

        ++index;
        while (index < normalized.size() && normalized.at(index).isSpace()) {
            ++index;
        }

        normalized = normalized.mid(index);
    }

    return normalized;
}
} // namespace

QString TerminalOutputHelper::normalizeMessage(const QString& message, TerminalSeverity severity)
{
    return stripMatchingLeadingSeverityToken(message, severity);
}

QString TerminalOutputHelper::formatHtmlLine(const QString& message,
                                             TerminalSeverity severity,
                                             const QDateTime& now)
{
    const QString timestamp = now.toString("HH:mm:ss");
    const QString normalizedMessage = normalizeMessage(message, severity);
    const QString prefix = severityPrefix(severity);

    QString line = QString("[%1] ").arg(timestamp);
    if (!prefix.isEmpty()) {
        line += prefix + " ";
    }
    line += normalizedMessage;

    const QString htmlLine = line.toHtmlEscaped();
    const QString cssClass = severityClass(severity);
    if (!cssClass.isEmpty()) {
        QString color;
        switch (severity) {
        case TerminalSeverity::Success:
            color = "#55ff55";
            break;
        case TerminalSeverity::Warning:
            color = "#ffff55";
            break;
        case TerminalSeverity::Error:
            color = "#ff5555";
            break;
        case TerminalSeverity::Info:
        default:
            break;
        }

        if (!color.isEmpty()) {
            return QString("<span class=\"%1\" style=\"color:%2;\">%3</span>")
                .arg(cssClass, color, htmlLine);
        }

        return QString("<span class=\"%1\">%2</span>").arg(cssClass, htmlLine);
    }

    return htmlLine;
}

void TerminalOutputHelper::append(QTextEdit* terminal,
                                  const QString& message,
                                  TerminalSeverity severity)
{
    if (!terminal) {
        return;
    }

    QTextCharFormat neutralFormat = terminal->currentCharFormat();
    neutralFormat.clearForeground();
    neutralFormat.setForeground(terminal->palette().color(QPalette::Text));

    // Prevent severity color state from carrying into subsequent plain/info lines.
    terminal->setCurrentCharFormat(neutralFormat);
    terminal->append(formatHtmlLine(message, severity));
    terminal->setCurrentCharFormat(neutralFormat);

    QTextCursor cursor = terminal->textCursor();
    cursor.movePosition(QTextCursor::End);
    terminal->setTextCursor(cursor);
}

QString TerminalOutputHelper::severityPrefix(TerminalSeverity severity)
{
    switch (severity) {
    case TerminalSeverity::Info:
        return QString();
    case TerminalSeverity::Success:
        return "SUCCESS:";
    case TerminalSeverity::Warning:
        return "WARNING:";
    case TerminalSeverity::Error:
        return "ERROR:";
    }

    return QString();
}

QString TerminalOutputHelper::severityClass(TerminalSeverity severity)
{
    switch (severity) {
    case TerminalSeverity::Info:
        return QString();
    case TerminalSeverity::Success:
        return "success";
    case TerminalSeverity::Warning:
        return "warning";
    case TerminalSeverity::Error:
        return "error";
    }

    return QString();
}
