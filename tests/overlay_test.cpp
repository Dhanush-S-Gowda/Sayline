#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>

class OverlayModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool shown MEMBER shown CONSTANT)
    Q_PROPERTY(bool busy MEMBER busy CONSTANT)
    Q_PROPERTY(QString status MEMBER status CONSTANT)
    Q_PROPERTY(QString transcript MEMBER transcript CONSTANT)
    Q_PROPERTY(double level MEMBER level CONSTANT)
    Q_PROPERTY(bool punctuationEnabled MEMBER punctuationEnabled NOTIFY changed)
public:
    bool shown = true, busy = false, punctuationEnabled = false;
    QString status = "Listening…", transcript = "Hello, world.";
    double level = 0.2;
    Q_INVOKABLE void setPunctuationEnabled(bool value) { punctuationEnabled = value; emit changed(); }
signals:
    void changed();
};
class OverlayTest : public QObject {
    Q_OBJECT
private slots:
    void hoverLeavesButtonClickable() {
        OverlayModel model;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("dictation", &model);
        engine.rootContext()->setContextProperty("previewMode", false);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto *button = window->findChild<QQuickItem *>("punctuationToggle");
        QVERIFY(button);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto point = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseMove(window, point);
        QTRY_VERIFY(button->property("hovered").toBool());
        QTest::qWait(1000); // Longer than the former tooltip delay.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_VERIFY(model.punctuationEnabled);
        QVERIFY(button->property("checked").toBool());
        window->grabWindow().save(QStringLiteral(SAYLINE_BINARY_DIR "/overlay-punctuation-on.png"));
        QTest::qWait(1000);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_VERIFY(!model.punctuationEnabled);
        QVERIFY(!button->property("checked").toBool());
        window->grabWindow().save(QStringLiteral(SAYLINE_BINARY_DIR "/overlay-punctuation-off.png"));
    }
};
QTEST_MAIN(OverlayTest)
#include "overlay_test.moc"
