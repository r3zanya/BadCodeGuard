#include "engine/Log.h"
#include "sys/Autostart.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>

namespace {

QtMessageHandler g_previousHandler = nullptr;

// Qt's own warnings and errors go to the program's log as well.
void forwardToLog(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg)
        bcg::log::warn(QStringLiteral("Qt: %1").arg(message));
    else if (type == QtCriticalMsg || type == QtFatalMsg)
        bcg::log::error(QStringLiteral("Qt: %1").arg(message));
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("BadCodeGuard"));
    QApplication::setApplicationName(QStringLiteral("BadCodeGuard"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    g_previousHandler = qInstallMessageHandler(forwardToLog);
    QApplication::setStyle(QStringLiteral("Fusion"));
    // The tray icon keeps the app alive after the window is closed.
    QApplication::setQuitOnLastWindowClosed(false);
    app.setStyleSheet(bcg::theme::styleSheet());

    MainWindow window;
    const bool autostart = app.arguments().contains(QString::fromLatin1(bcg::autostart::kFlag));
    if (!window.startMinimized(autostart))
        window.show();

    return app.exec();
}
