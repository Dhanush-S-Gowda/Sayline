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
            width: parent.width - x - 80
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
            anchors.rightMargin: 40
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
        ToolButton {
            id: commandHelp
            objectName: "commandHelp"
            width: 28; height: 32
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "?"
            font.bold: true
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            Accessible.name: "Spoken command reference"
        }
        ProgressBar {
            x: 95; y: parent.height - 6
            width: parent.width - x - 80
            height: 2
            visible: !previewMode && dictation.busy
            indeterminate: visible
            Accessible.name: "Preparing dictation"
        }
    }
    Window {
        id: commandGuide
        objectName: "commandGuide"
        transientParent: root
        flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
        property bool opened: false
        visible: root.visible && opened
        screen: root.screen
        width: Math.min(700, root.screen.width - 24)
        height: Math.min(guideContent.implicitHeight + 24, 560,
                         Math.max(root.y - root.screen.virtualY - 8,
                                  root.screen.virtualY + root.screen.height - root.y - root.height - 8))
        x: Math.max(root.screen.virtualX + 8,
                    Math.min(root.x + root.width - width, root.screen.virtualX + root.screen.width - width - 8))
        y: root.y >= height + root.screen.virtualY + 8 ? root.y - height - 4 : root.y + root.height + 4
        color: theme.window
        HoverHandler { id: guideHover }
        ScrollView {
            id: guideScroll
            anchors.fill: parent
            anchors.margins: 12
            clip: true
            contentWidth: availableWidth
            Column {
                id: guideContent
                width: guideScroll.availableWidth
                spacing: 8
                Label {
                    text: "Spoken commands"
                    font.bold: true
                    color: theme.windowText
                }
                Label {
                    width: parent.width
                    text: "Requires punctuation on · Say “literal” before a command to keep its words."
                    wrapMode: Text.WordWrap
                    color: Qt.alpha(theme.windowText, 0.75)
                }
                Row {
                    width: parent.width
                    spacing: 10
                    Repeater {
                        model: [
                            [
                                ["Say", "Output"],
                                ["comma", ","], ["full stop", "."],
                                ["question mark", "?"], ["exclamation mark", "!"],
                                ["colon / semicolon", ": / ;"],
                                ["hyphen / em dash", "- joins / — spaced"],
                                ["ellipsis / apostrophe", "… / '"],
                                ["open / close quote", "\""],
                                ["open / close parenthesis", "( / )"],
                                ["open / close bracket", "[ / ]"],
                                ["forward slash / backslash", "/ / \\"],
                                ["underscore / at sign", "_ / @"],
                                ["hash sign / percent sign", "# / %"],
                                ["ampersand", "& (spaced)"]
                            ],
                            [
                                ["Say", "Output"],
                                ["space", "One space"],
                                ["backspace", "Delete one character"],
                                ["new line / next line", "Line break"],
                                ["new / next paragraph", "Blank line"],
                                ["zero … nine", "0 … 9"],
                                ["point + digit", "Decimal: 0.2"],
                                ["caps lock on", "UPPERCASE"],
                                ["caps lock off / of", "Normal casing"],
                                ["start key point(s)", "Start - item"],
                                ["next point", "Next - item"],
                                ["stop key point / keypoint", "End list; new line"],
                                ["literal comma", "comma (example)"]
                            ]
                        ]
                        delegate: Column {
                            required property var modelData
                            width: (guideContent.width - 10) / 2
                            Repeater {
                                model: parent.modelData
                                delegate: Rectangle {
                                    id: commandRow
                                    required property var modelData
                                    required property int index
                                    width: parent.width
                                    height: Math.max(phraseLabel.implicitHeight, outputLabel.implicitHeight) + 12
                                    color: Qt.alpha(theme.windowText, index === 0 ? 0.12 : (index % 2 ? 0.035 : 0.065))
                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 10
                                        Label {
                                            id: phraseLabel
                                            width: (commandRow.width - 22) * 0.57
                                            text: commandRow.modelData[0]
                                            textFormat: Text.PlainText
                                            wrapMode: Text.WordWrap
                                            font.pointSize: Math.max(9, Qt.application.font.pointSize - 1)
                                            font.bold: commandRow.index === 0
                                            color: theme.windowText
                                        }
                                        Label {
                                            id: outputLabel
                                            width: (commandRow.width - 22) * 0.43
                                            text: commandRow.modelData[1]
                                            textFormat: Text.PlainText
                                            wrapMode: Text.WordWrap
                                            font.pointSize: phraseLabel.font.pointSize
                                            font.bold: commandRow.index === 0
                                            color: theme.windowText
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                Label {
                    width: parent.width
                    text: "Only “start key point” enables lists; “stop key point” ends them. Sentence and line starts are capitalized. Joined spellings work too."
                    wrapMode: Text.WordWrap
                    color: Qt.alpha(theme.windowText, 0.75)
                }
                Label {
                    width: parent.width
                    text: "v" + Qt.application.version
                    visible: Qt.application.version.length > 0
                    horizontalAlignment: Text.AlignRight
                    color: Qt.alpha(theme.windowText, 0.6)
                }
            }
        }
    }
    Timer {
        id: showGuide
        interval: 2000
        onTriggered: {
            if (root.visible && commandHelp.hovered) commandGuide.opened = true
        }
    }
    Timer {
        id: hideGuide
        interval: 250
        onTriggered: commandGuide.opened = false
    }
    onVisibleChanged: {
        if (!visible) {
            showGuide.stop()
            hideGuide.stop()
            commandGuide.opened = false
        }
    }
    Connections {
        target: commandHelp
        function onHoveredChanged() {
            if (commandHelp.hovered) {
                hideGuide.stop()
                if (!commandGuide.opened) showGuide.restart()
            } else {
                showGuide.stop()
                if (commandGuide.opened) hideGuide.restart()
            }
        }
    }
    Connections {
        target: guideHover
        function onHoveredChanged() {
            if (guideHover.hovered) hideGuide.stop()
            else if (!commandHelp.hovered && commandGuide.opened) hideGuide.restart()
        }
    }
    onClosing: function(close) {
        if (!previewMode && dictation.shown) {
            close.accepted = false
            dictation.Toggle()
        }
    }
}
