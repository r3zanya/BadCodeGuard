#include "RuleEngine.h"

#include <QRegularExpression>

namespace bcg {

namespace {

// Decodes the body of a quoted string (supports \" \\ \n \r \t \xHH).
bool decodeString(const QString& quoted, QByteArray& out)
{
    out.clear();
    if (quoted.size() < 2 || !quoted.startsWith(QLatin1Char('"')) || !quoted.endsWith(QLatin1Char('"')))
        return false;
    const QString body = quoted.mid(1, quoted.size() - 2);
    for (int i = 0; i < body.size(); ++i) {
        const QChar c = body[i];
        if (c != QLatin1Char('\\')) {
            out += QString(c).toUtf8();
            continue;
        }
        if (++i >= body.size())
            return false;
        switch (body[i].unicode()) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'x': {
            bool ok = false;
            const int v = body.mid(i + 1, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            out += static_cast<char>(v);
            i += 2;
            break;
        }
        default: out += QString(body[i]).toUtf8(); break;
        }
    }
    return !out.isEmpty();
}

} // namespace

int RuleEngine::loadText(const QString& text, QString* error)
{
    static const QRegularExpression ruleStart(
        QStringLiteral("^rule\\s+([A-Za-z0-9_.\\-]+)\\s*\\{$"));
    static const QRegularExpression patternLine(
        QStringLiteral("^\\$([A-Za-z0-9_]+)\\s*=\\s*(.+)$"));
    static const QRegularExpression keyLine(QStringLiteral("^([a-z]+)\\s*=\\s*(.+)$"));

    int added = 0;
    bool inRule = false;
    Rule rule;
    int lineNo = 0;
    auto fail = [&](const QString& msg) {
        if (error)
            *error = QStringLiteral("строка %1: %2").arg(lineNo).arg(msg);
        return added;
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        ++lineNo;
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QStringLiteral("//")))
            continue;

        if (!inRule) {
            const auto m = ruleStart.match(line);
            if (!m.hasMatch())
                return fail(QStringLiteral("ожидалось «rule Имя {»"));
            rule = Rule();
            rule.name = m.captured(1);
            rule.threat = rule.name;
            inRule = true;
            continue;
        }
        if (line == QLatin1String("}")) {
            if (rule.patterns.isEmpty())
                return fail(QStringLiteral("в правиле %1 нет шаблонов").arg(rule.name));
            if (rule.need > rule.patterns.size())
                return fail(QStringLiteral("условие требует больше шаблонов, чем есть"));
            m_rules.append(rule);
            ++added;
            inRule = false;
            continue;
        }

        const auto p = patternLine.match(line);
        if (p.hasMatch()) {
            Pattern pat;
            QString value = p.captured(2).trimmed();
            if (value.startsWith(QLatin1Char('{'))) {
                const int end = value.indexOf(QLatin1Char('}'));
                if (end < 0)
                    return fail(QStringLiteral("не закрыта «{»"));
                const QStringList bytes = value.mid(1, end - 1).simplified().split(QLatin1Char(' '),
                                                                                    Qt::SkipEmptyParts);
                bool anyWildcard = false;
                for (const QString& b : bytes) {
                    if (b == QLatin1String("??")) {
                        pat.bytes += '\0';
                        pat.wildcard += '\1';
                        anyWildcard = true;
                        continue;
                    }
                    bool ok = false;
                    const int v = b.toInt(&ok, 16);
                    if (!ok || b.size() != 2)
                        return fail(QStringLiteral("неверный байт «%1»").arg(b));
                    pat.bytes += static_cast<char>(v);
                    pat.wildcard += '\0';
                }
                if (!anyWildcard)
                    pat.wildcard.clear();
                if (pat.bytes.isEmpty())
                    return fail(QStringLiteral("пустой hex-шаблон"));
            } else {
                QString flags;
                const int closing = value.lastIndexOf(QLatin1Char('"'));
                if (closing > 0) {
                    flags = value.mid(closing + 1).trimmed();
                    value = value.left(closing + 1);
                }
                if (!decodeString(value, pat.bytes))
                    return fail(QStringLiteral("неверная строка"));
                pat.nocase = flags == QLatin1String("nocase");
                if (pat.nocase)
                    pat.bytes = pat.bytes.toLower();
            }
            rule.patterns.append(pat);
            continue;
        }

        const auto k = keyLine.match(line);
        if (!k.hasMatch())
            return fail(QStringLiteral("непонятная строка «%1»").arg(line));
        const QString key = k.captured(1);
        const QString value = k.captured(2).trimmed();
        if (key == QLatin1String("threat")) {
            QByteArray name;
            if (!decodeString(value, name))
                return fail(QStringLiteral("threat должен быть в кавычках"));
            rule.threat = QString::fromUtf8(name);
        } else if (key == QLatin1String("maxsize")) {
            bool ok = false;
            rule.maxSize = value.toLongLong(&ok);
            if (!ok || rule.maxSize <= 0)
                return fail(QStringLiteral("maxsize должен быть числом"));
        } else if (key == QLatin1String("condition")) {
            if (value == QLatin1String("any")) {
                rule.need = 0;
            } else if (value == QLatin1String("all")) {
                rule.need = -1;
            } else {
                bool ok = false;
                rule.need = value.toInt(&ok);
                if (!ok || rule.need < 1)
                    return fail(QStringLiteral("condition: any, all или число"));
            }
        } else {
            return fail(QStringLiteral("неизвестный параметр «%1»").arg(key));
        }
    }
    if (inRule)
        return fail(QStringLiteral("не закрыто правило %1").arg(rule.name));
    return added;
}

bool RuleEngine::find(const QByteArray& data, const QByteArray& lowered, const Pattern& p)
{
    const QByteArray& hay = p.nocase ? lowered : data;
    if (p.wildcard.isEmpty())
        return hay.indexOf(p.bytes) >= 0;

    // Anchor on the first fixed byte, then compare the rest with the mask.
    qsizetype anchor = 0;
    while (anchor < p.bytes.size() && p.wildcard[anchor])
        ++anchor;
    if (anchor == p.bytes.size())
        return hay.size() >= p.bytes.size(); // all wildcards
    const qsizetype last = hay.size() - p.bytes.size();
    for (qsizetype pos = hay.indexOf(p.bytes[anchor], anchor); pos >= 0 && pos - anchor <= last;
         pos = hay.indexOf(p.bytes[anchor], pos + 1)) {
        const qsizetype start = pos - anchor;
        bool ok = true;
        for (qsizetype i = 0; i < p.bytes.size() && ok; ++i)
            ok = p.wildcard[i] || hay[start + i] == p.bytes[i];
        if (ok)
            return true;
    }
    return false;
}

QString RuleEngine::match(const QByteArray& data, qint64 fileSize) const
{
    QByteArray lowered; // only built if a rule needs it
    for (const Rule& rule : m_rules) {
        if (rule.maxSize > 0 && fileSize > rule.maxSize)
            continue;
        int hits = 0;
        const int need = rule.need == 0 ? 1 : (rule.need < 0 ? int(rule.patterns.size()) : rule.need);
        for (const Pattern& p : rule.patterns) {
            if (p.nocase && lowered.isEmpty())
                lowered = data.toLower();
            if (find(data, lowered, p) && ++hits >= need)
                return rule.threat;
        }
    }
    return {};
}

} // namespace bcg
