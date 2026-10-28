#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace bcg {

// Small YARA-like pattern rules. One statement per line:
//
//   rule Eicar {
//     threat = "EICAR-Test-File"        (optional, defaults to the rule name)
//     maxsize = 1024                    (optional: only files up to N bytes)
//     $a = "EICAR-STANDARD" nocase      (text; "nocase" is optional)
//     $b = { 4D 5A ?? 00 }              (hex bytes, ?? = any byte)
//     condition = any | all | 2         (default: any)
//   }
//
// Lines starting with '#' or '//' are comments.
class RuleEngine {
public:
    // Parses rules and appends them. Returns the count; on a syntax error the
    // message (with line number) is stored in `error` and parsing stops.
    int loadText(const QString& text, QString* error = nullptr);
    qsizetype size() const { return m_rules.size(); }

    // Name of the first matching rule's threat, or empty. `fileSize` is the
    // real size (data may be only the beginning of the file).
    QString match(const QByteArray& data, qint64 fileSize) const;

private:
    struct Pattern {
        QByteArray bytes;
        QByteArray wildcard; // same length as bytes, 1 = any byte; empty = none
        bool nocase = false;
    };
    struct Rule {
        QString name;
        QString threat;
        QList<Pattern> patterns;
        int need = 0; // 0 = any, -1 = all, N = at least N
        qint64 maxSize = 0;
    };
    static bool find(const QByteArray& data, const QByteArray& lowered, const Pattern& p);

    QList<Rule> m_rules;
};

} // namespace bcg
