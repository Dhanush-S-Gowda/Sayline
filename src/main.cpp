#include "controller.h"
#include "runtime.h"
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QScreen>
#include <QSocketNotifier>
#include <QTimer>
#include <csignal>
#include <sys/signalfd.h>
#include <unistd.h>

int main(int argc, char **argv) {
    // Timer-driven retention must not initialize a GUI, microphone, or model.
    if (argc == 2 && (QByteArray(argv[1]) == "--prune-history" || QByteArray(argv[1]) == "--migrate-state")) {
        QString error;
        if (!migrateLegacyState(&error)) { fprintf(stderr, "%s\n", qPrintable(error)); return 1; }
        if (QByteArray(argv[1]) == "--prune-history") pruneSessionHistory();
        return 0;
    }
    // Route process termination into the Qt event loop so owned children are reaped.
    sigset_t mask;
    sigemptyset(&mask); sigaddset(&mask, SIGTERM); sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
    const int signalFd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Sayline");
    app.setApplicationName("sayline");
    app.setApplicationVersion("0.4.0");
    app.setDesktopFileName("org.sayline.Sayline");
    app.setWindowIcon(QIcon(":/sayline-icon.ico"));
    app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser;
    parser.setApplicationDescription("Sayline: local dictation for Fedora KDE Wayland");
    parser.addHelpOption();
    parser.addOption({"migrate-state", "Move previous-version logs and sessions to Sayline's state directory."});
    parser.addOption({"prune-history", "Remove session files older than 24 hours and exit."});
    parser.addOption({"toggle", "Toggle the overlay and microphone (also starts the app)."});
    parser.addOption({"quit", "Stop the running app and its owned speech process."});
    parser.addOption({"background", "Start hidden for D-Bus shortcut activation."});
    parser.addOption({"preview", "Show only the overlay; do not record, type, or start the model."});
    parser.addOption({"smoke-test", "Run a short overlay-only startup check and exit."});
    parser.process(app);
    const bool preview = parser.isSet("preview") || parser.isSet("smoke-test");
    auto bus = QDBusConnection::sessionBus();
    if (!preview) {
        if (!bus.isConnected()) { qCritical("A session D-Bus connection is required."); return 1; }
        if (!bus.registerService("org.sayline.Sayline")) {
            QDBusInterface running("org.sayline.Sayline", "/Sayline", "org.sayline.Sayline", bus);
            auto result = running.call(parser.isSet("quit") ? "Quit" : "Toggle");
            if (result.type() == QDBusMessage::ErrorMessage) {
                qCritical().noquote() << result.errorMessage(); return 1;
            }
            return 0;
        }
        if (parser.isSet("quit")) return 0;
    }
    if (!preview) initializeRuntimeLog();
    Controller controller;
    if (!preview && !bus.registerObject("/Sayline", &controller, QDBusConnection::ExportAllSlots)) {
        qCritical("Cannot register the dictation control object."); return 1;
    }
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("dictation", &controller);
    engine.rootContext()->setContextProperty("previewMode", preview);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) return 1;
    // Qt's xdg-shell mapping disables keyboard focus for this flag.
    window->setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    QObject::connect(window, &QWindow::visibleChanged, &controller, [window](bool visible) {
        runtimeEvent("overlay.visibility", {{"visible", visible}, {"state", int(window->windowState())}});
    });
    QObject::connect(window, &QWindow::windowStateChanged, &controller, [window, &controller](Qt::WindowState state) {
        runtimeEvent("overlay.window_state", {{"state", int(state)}});
        if (state == Qt::WindowMinimized && controller.shown()) window->setWindowState(Qt::WindowNoState);
    });
    if (auto *screen = app.primaryScreen()) {
        const auto area = screen->availableGeometry();
        window->setPosition(area.center().x() - window->width() / 2, area.y() + area.height() - window->height() - qBound(64, qRound(area.height() * 0.08), 120));
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, &Controller::shutdown);
    std::unique_ptr<QSocketNotifier> signalNotifier;
    if (signalFd >= 0) {
        signalNotifier = std::make_unique<QSocketNotifier>(signalFd, QSocketNotifier::Read);
        QObject::connect(signalNotifier.get(), &QSocketNotifier::activated, &controller, [&controller, signalFd] {
            signalfd_siginfo info{};
            if (read(signalFd, &info, sizeof(info)) == sizeof(info)) controller.Quit();
        });
    }
    if (!preview) {
        controller.initializeFocusBridge();
        // Give the focus bridge time to report the target before recording begins.
        if (!parser.isSet("background")) QTimer::singleShot(200, &controller, &Controller::Toggle);
    }
    if (parser.isSet("smoke-test")) QTimer::singleShot(700, &app, &QCoreApplication::quit);
    const int result = app.exec();
    runtimeEvent("application.exit", {{"code", result}});
    if (signalFd >= 0) close(signalFd);
    return result;
}
