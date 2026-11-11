import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root
    width: 460
    height: 48
    minimumWidth: 360
    minimumHeight: 48
    maximumHeight: 48
    color: "transparent"
    title: "Sayline"
    visible: previewMode || dictation.shown
    // A utility/Tool window can follow inactive-app hiding policies. Use an
    // independent top-level; the KWin bridge supplies skip-taskbar and keep-above.
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    SystemPalette { id: theme; colorGroup: SystemPalette.Active }
    readonly property bool darkSurface: theme.window.hslLightness < 0.5

    // Blend the light into the rounded surface; avoid a bright one-pixel seam.
    Rectangle {
        x: 2; y: 4; width: parent.width - 4; height: parent.height - 4
        radius: 15; color: "#16000000"
    }
    Rectangle {
        x: 4; y: 3; width: parent.width - 8; height: parent.height - 4
        radius: 14; color: "#24000000"
    }
    Rectangle {
        id: body
        x: 4; y: 1; width: parent.width - 8; height: parent.height - 5
        radius: 14
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.tint(theme.window, Qt.rgba(1, 1, 1, 0.065)) }
            GradientStop { position: 0.28; color: Qt.tint(theme.window, Qt.rgba(1, 1, 1, 0.025)) }
            GradientStop { position: 0.55; color: theme.window }
            GradientStop { position: 1; color: Qt.darker(theme.window, 1.06) }
        }
        border.width: 1
        border.color: root.darkSurface
            ? Qt.tint(theme.window, Qt.rgba(1, 1, 1, 0.08))
            : Qt.darker(theme.window, 1.12)
        MouseArea {
            id: dragArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeAllCursor
            onPressed: root.startSystemMove()
            Accessible.name: "Move dictation window"
        }

        Row {
            width: 59
            height: 26
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Repeater {
                model: 11
                Rectangle {
                    required property int index
                    width: 2.5
                    height: 3 + (previewMode ? 0.55 : dictation.level) * (6 + 18 * Math.abs(Math.sin(index * 1.3 + waveform.phase)))
                    radius: 1.25
                    anchors.verticalCenter: parent.verticalCenter
                    color: theme.highlight
                    Behavior on height { NumberAnimation { duration: 85 } }
                }
            }
        }
        QtObject { id: waveform; property real phase: 0 }
        Timer {
            interval: 90
            repeat: true
            running: root.visible && (previewMode || dictation.level > 0.01)
            onTriggered: waveform.phase += 0.4
        }
        Rectangle {
            x: 82; width: 1; height: 22
            anchors.verticalCenter: parent.verticalCenter
            color: Qt.alpha(theme.windowText, 0.16)
        }
        Label {
            objectName: "transcriptLabel"
            x: 95
            width: parent.width - x - 48
            anchors.verticalCenter: parent.verticalCenter
            font: Qt.application.font
            color: theme.windowText
            text: punctuationToggle.hovered && !previewMode
                ? (dictation.punctuationEnabled ? "Spoken punctuation on" : "Spoken punctuation off")
                : (previewMode ? "I'm in the middle of speaking…"
                    : (dictation.transcript || dictation.status).replace(/[\r\n\u2028\u2029]+/g, " "))
            textFormat: Text.PlainText
            wrapMode: Text.NoWrap
            maximumLineCount: 1
            elide: Text.ElideLeft
            Accessible.name: text
        }
        ToolButton {
            id: punctuationToggle
            width: 32; height: 32
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            objectName: "punctuationToggle"
            icon.source: "qrc:/qml/icons/punctuation.svg"
            icon.width: 20
            icon.height: 20
            icon.color: checked ? theme.highlightedText : theme.windowText
            background: Rectangle {
                radius: 8
                color: punctuationToggle.checked ? theme.highlight
                    : Qt.alpha(theme.windowText, punctuationToggle.hovered ? 0.14 : 0.06)
                border.width: 1
                border.color: punctuationToggle.checked ? theme.highlight : Qt.alpha(theme.windowText, 0.16)
            }
            checkable: true
            checked: !previewMode && dictation.punctuationEnabled
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            enabled: !previewMode
            onClicked: dictation.setPunctuationEnabled(checked)
            Accessible.name: "Spoken punctuation"
        }
        ProgressBar {
            x: 95; y: parent.height - 6
            width: parent.width - x - 48
            height: 2
            visible: !previewMode && dictation.busy
            indeterminate: visible
            Accessible.name: "Preparing dictation"
        }
    }
    onClosing: function(close) {
        if (!previewMode && dictation.shown) {
            close.accepted = false
            dictation.Toggle()
        }
    }
}
