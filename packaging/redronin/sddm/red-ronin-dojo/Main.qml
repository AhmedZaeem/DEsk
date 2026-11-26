// RED RONIN · the dojo's gate, as the login (SDDM)
//
// The picture is DEsk's own lock screen at rest on a rainy night
// (assets/door.png, made by scripts/redronin-greeter.sh with desk-preview),
// so the login and the lock are the same place. The password goes into the
// slot of the lacquer board on the door: a real field laid exactly over it
// (theme.conf slotX/Y/W/H, as fractions of the picture), its letters shown
// as the lock shows them, amber lights in the dark glass.
import QtQuick

Rectangle {
    id: root
    width: 1920; height: 1080
    color: "#050404"

    readonly property color amber: "#FFB04A"
    readonly property color lacquerRed: "#7A0C10"
    readonly property color paper: "#EFE6D2"
    readonly property string serif: "Noto Serif CJK JP"

    property int sessionIndex: sessionModel.lastIndex
    property bool picked: false
    property bool failed: false

    // the sessions' names, through the model's "name" role (it has no DisplayRole)
    property var sessionNames: []
    Repeater {
        model: sessionModel
        delegate: Item {
            visible: false
            Component.onCompleted: {
                var a = root.sessionNames.slice()
                a[index] = model.name
                root.sessionNames = a
            }
        }
    }
    // who logs in: the one SDDM remembers, else the first it lists (a first
    // boot remembers nobody)
    property string firstUser: ""
    Repeater {
        model: userModel
        delegate: Item {
            visible: false
            Component.onCompleted: if (index === 0) root.firstUser = model.name
        }
    }
    readonly property string who: userModel.lastUser ? userModel.lastUser : firstUser

    // DEsk first (theme.conf defaultSession), unless somebody chooses
    onSessionNamesChanged: {
        if (picked) return
        var want = config.defaultSession || "DEsk"
        var i = sessionNames.indexOf(want)
        if (i >= 0) { sessionIndex = i; picked = true }
    }

    // ── the door, filling the screen as the lock does ──
    Image {
        id: door
        anchors.fill: parent
        source: "assets/door.png"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        // where the picture's pixels land, for the slot
        readonly property real k: Math.max(root.width / sourceSize.width, root.height / sourceSize.height)
        readonly property real ox: (root.width - sourceSize.width * k) / 2
        readonly property real oy: (root.height - sourceSize.height * k) / 2
    }

    // ── the slot ──
    Item {
        id: slot
        x: door.ox + Number(config.slotX) * door.sourceSize.width * door.k
        y: door.oy + Number(config.slotY) * door.sourceSize.height * door.k
        width: Number(config.slotW) * door.sourceSize.width * door.k
        height: Number(config.slotH) * door.sourceSize.height * door.k

        SequentialAnimation {
            id: shake
            NumberAnimation { target: slotFace; property: "x"; to: 10; duration: 50 }
            NumberAnimation { target: slotFace; property: "x"; to: -8; duration: 50 }
            NumberAnimation { target: slotFace; property: "x"; to: 5; duration: 50 }
            NumberAnimation { target: slotFace; property: "x"; to: 0; duration: 50 }
        }

        Rectangle {
            id: slotFace
            width: parent.width; height: parent.height
            radius: 2
            color: root.failed ? root.lacquerRed : "#0B0807"
            Behavior on color { ColorAnimation { duration: 160 } }
            border.width: 1
            border.color: "#3A2A14"

            // the letters: a light each, as on the door
            Row {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.height * 0.35
                spacing: parent.height * 0.18
                visible: !root.failed
                Repeater {
                    model: Math.min(pw.text.length, 16)
                    Rectangle {
                        width: slotFace.height * 0.30; height: width; radius: width / 2
                        color: root.amber
                        opacity: 0.95
                        Rectangle {   // the glow round it
                            anchors.centerIn: parent
                            width: parent.width * 2.2; height: width; radius: width / 2
                            color: Qt.rgba(1.0, 0.62, 0.22, 0.16)
                        }
                    }
                }
                // the caret, where the next goes
                Rectangle {
                    width: Math.max(2, slotFace.height * 0.05); height: slotFace.height * 0.5
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.amber
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite; running: true
                        NumberAnimation { to: 0.1; duration: 520 }
                        NumberAnimation { to: 1.0; duration: 520 }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: root.failed
                text: "違います · That is not it"
                color: "#F4D9B0"; font.family: root.serif; font.pixelSize: slotFace.height * 0.42
            }
        }

        TextInput {
            id: pw
            anchors.fill: parent
            echoMode: TextInput.Password
            color: "transparent"
            cursorVisible: false
            selectionColor: "transparent"
            focus: true
            maximumLength: 256
            onTextChanged: if (text.length) root.failed = false
            onAccepted: root.doLogin()
        }

        Text {
            anchors { top: parent.bottom; topMargin: parent.height * 0.25; horizontalCenter: parent.horizontalCenter }
            visible: keyboard.capsLock
            text: "Caps Lock"
            color: root.paper; opacity: 0.8
            font.family: root.serif; font.pixelSize: Math.max(12, slot.height * 0.32)
        }
    }

    // ── the hour, top left, small ──
    Column {
        anchors { left: parent.left; top: parent.top; leftMargin: 54; topMargin: 42 }
        spacing: 2
        Text {
            id: clock
            text: Qt.formatDateTime(new Date(), "HH:mm")
            color: root.paper; opacity: 0.88
            font.family: root.serif; font.pixelSize: 44
        }
        Text {
            id: dateT
            text: Qt.formatDateTime(new Date(), "dddd, d MMMM")
            color: root.paper; opacity: 0.55
            font.family: root.serif; font.pixelSize: 15
        }
    }
    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: {
            clock.text = Qt.formatDateTime(new Date(), "HH:mm")
            dateT.text = Qt.formatDateTime(new Date(), "dddd, d MMMM")
        }
    }

    // ── wooden tags along the foot: the session, and the power ──
    component Tag: Rectangle {
        id: tag
        property string label: ""
        signal pressed(var mouse)
        height: 34; radius: 3
        width: tagText.implicitWidth + 30
        color: tma.containsMouse ? "#3B2414" : "#22150C"
        border.width: 1; border.color: "#5A3A1E"
        Behavior on color { ColorAnimation { duration: 140 } }
        Text {
            id: tagText
            anchors.centerIn: parent
            text: tag.label
            color: root.paper; font.family: root.serif; font.pixelSize: 14
        }
        MouseArea {
            id: tma
            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: function(mouse) { tag.pressed(mouse) }
        }
    }

    Tag {
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 54; bottomMargin: 40 }
        label: (root.sessionNames[root.sessionIndex] || "DEsk") + "  ›"
        onPressed: function(mouse) {
            var n = Math.max(1, sessionModel.rowCount())
            root.sessionIndex = (root.sessionIndex + (mouse.button === Qt.RightButton ? n - 1 : 1)) % n
            root.picked = true
        }
    }

    Row {
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 54; bottomMargin: 40 }
        spacing: 10
        Tag { label: "寝 · Sleep"; visible: sddm.canSuspend; onPressed: sddm.suspend() }
        Tag { label: "再 · Restart"; onPressed: sddm.reboot() }
        Tag { label: "終 · Shut down"; onPressed: sddm.powerOff() }
    }

    function doLogin() {
        root.failed = false
        sddm.login(root.who, pw.text, root.sessionIndex)
    }

    Connections {
        target: sddm
        function onLoginFailed() {
            root.failed = true
            pw.text = ""
            pw.forceActiveFocus()
            shake.start()
            failTimer.restart()
        }
    }
    Timer { id: failTimer; interval: 2200; onTriggered: root.failed = false }

    Component.onCompleted: pw.forceActiveFocus()
}
