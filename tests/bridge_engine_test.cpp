#include <QtTest>
#include <QJSEngine>
#include <QFile>
#include <QUuid>

class MockWindow : public QObject {
    Q_OBJECT
    Q_PROPERTY(QByteArray resourceClass MEMBER resourceClass)
    Q_PROPERTY(QUuid internalId MEMBER internalId)
    Q_PROPERTY(int pid MEMBER pid)
    Q_PROPERTY(bool keepAbove READ keepAbove WRITE setKeepAbove NOTIFY keepAboveChanged)
    Q_PROPERTY(bool minimized READ minimized WRITE setMinimized NOTIFY minimizedChanged)
    Q_PROPERTY(bool skipTaskbar MEMBER skipTaskbar)
    Q_PROPERTY(bool skipPager MEMBER skipPager)
    Q_PROPERTY(bool skipSwitcher MEMBER skipSwitcher)
    Q_PROPERTY(QList<QObject*> desktops MEMBER desktops)
    Q_PROPERTY(QRectF frameGeometry MEMBER frameGeometry)
public:
    QByteArray resourceClass;
    QUuid internalId = QUuid::createUuid();
    int pid = 0;
    bool skipTaskbar = false, skipPager = false, skipSwitcher = false;
    QList<QObject*> desktops;
    QRectF frameGeometry{0, 0, 460, 48};
    QRectF usableArea{0, 0, 1920, 1080};
    bool keepAbove() const { return above; }
    bool minimized() const { return mini; }
    void setKeepAbove(bool value) { if (above != value) { above = value; emit keepAboveChanged(); } }
    void setMinimized(bool value) { if (mini != value) { mini = value; emit minimizedChanged(); } }
private:
    bool above = false, mini = false;
signals:
    void keepAboveChanged();
    void minimizedChanged();
};
class MockWorkspace : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject*> stackingOrder MEMBER windows)
    Q_PROPERTY(QObject* activeWindow MEMBER active)
public:
    QList<QObject*> windows;
    QObject *active = nullptr;
    Q_INVOKABLE QRectF clientArea(int kind, QObject *target) const {
        if (kind == 0) return {0, 0, 4480, 1500}; // Combined WorkArea must not be used.
        return qobject_cast<MockWindow *>(target)->usableArea;
    }
signals:
    void windowAdded(QObject *window);
    void windowActivated(QObject *window);
};
class BridgeEngineTest : public QObject {
    Q_OBJECT
private slots:
    void actualQtTypesAndSignals() {
        MockWindow overlay, editor, otherEditor;
        overlay.pid = 1001; overlay.resourceClass = "unexpected-wayland-app-id";
        editor.pid = 1002; editor.resourceClass = "dev.zed.Zed";
        editor.usableArea = {1920, 100, 2560, 1400};
        otherEditor.pid = 1003; otherEditor.resourceClass = "dev.zed.Zed";
        MockWorkspace workspace;
        workspace.windows = {&overlay, &editor, &otherEditor}; workspace.active = &otherEditor;
        QJSEngine::setObjectOwnership(&overlay, QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(&editor, QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(&otherEditor, QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(&workspace, QJSEngine::CppOwnership);
        QJSEngine engine;
        // KWin installs this conversion for script assignments to frameGeometry.
        if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QRectF>())
            QMetaType::registerConverter<QJSValue, QRectF>([](const QJSValue &value) {
                return QRectF(value.property("x").toNumber(), value.property("y").toNumber(),
                              value.property("width").toNumber(), value.property("height").toNumber());
            });
        auto kwin = engine.newObject();
        kwin.setProperty("WorkArea", 0);
        kwin.setProperty("MaximizeArea", 1);
        engine.globalObject().setProperty("KWin", kwin);
        engine.globalObject().setProperty("workspace", engine.newQObject(&workspace));
        engine.globalObject().setProperty("capturedTarget", editor.internalId.toString());
        QVERIFY(!engine.evaluate("var calls=[]; function callDBus(){ calls.push(Array.from(arguments)); if(arguments[3] === 'PlacementTarget') arguments[4](capturedTarget); }").isError());
        QFile script(QStringLiteral(SAYLINE_SOURCE_DIR) + "/packaging/focus.js");
        QVERIFY(script.open(QIODevice::ReadOnly));
        auto source = script.readAll();
        source.replace("@APP_PID@", "1001");
        source.replace("@DBUS_OWNER@", ":1.123");
        source.replace("@BRIDGE_TOKEN@", "private-test-token");
        const auto result = engine.evaluate(QString::fromUtf8(source));
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        QVERIFY(overlay.keepAbove());
        QVERIFY(overlay.skipTaskbar);
        QCOMPARE(overlay.frameGeometry, QRectF(2970, 1340, 460, 48));
        overlay.frameGeometry.moveTopLeft({200, 300});
        QVERIFY(!editor.keepAbove());
        const auto focus = engine.evaluate("calls.find(c => c[3] === 'Focus')");
        QCOMPARE(focus.property(0).toString(), QString(":1.123"));
        QCOMPARE(focus.property(5).toString(), QString("dev.zed.Zed"));
        QCOMPARE(focus.property(6).toString(), QString("private-test-token"));
        QVERIFY(engine.evaluate("calls.some(c => c[3] === 'BridgeEvent' && c[4] === 'ready')").toBool());
        overlay.setMinimized(true);
        QVERIFY(!overlay.minimized());
        overlay.setKeepAbove(false);
        QVERIFY(overlay.keepAbove());
        emit workspace.windowActivated(&editor);
        QCOMPARE(overlay.frameGeometry.topLeft(), QPointF(200, 300));
        QCOMPARE(engine.evaluate("calls.filter(c => c[3] === 'Focus').length").toInt(), 2);
    }
};
QTEST_GUILESS_MAIN(BridgeEngineTest)
#include "bridge_engine_test.moc"
