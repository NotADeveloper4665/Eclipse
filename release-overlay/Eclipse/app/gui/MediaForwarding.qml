import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

Column {
    id: row
    property string title
    property string deviceKind: "microphone"
    property string sshTarget
    spacing: 6
    property var forwarding
    Component.onCompleted: forwarding.refresh()
    Label { text: row.title; font.bold: true }
    ComboBox {
        id: picker
        width: parent.width
        model: forwarding.inputs
        textRole: "label"
        enabled: forwarding.supported && !forwarding.busy && !forwarding.sharing
    }
    TextField {
        id: hostCamera
        width: parent.width
        visible: row.deviceKind === "camera"
        text: "/dev/video42"
        placeholderText: qsTr("Host virtual camera device")
        enabled: !forwarding.busy && !forwarding.sharing
    }
    Flow {
        spacing: 6
        width: parent.width
        Button { text: qsTr("Refresh"); enabled: !forwarding.busy && !forwarding.sharing; onClicked: forwarding.refresh() }
        Button {
            text: qsTr("Start forwarding")
            enabled: forwarding.supported && !forwarding.busy && !forwarding.sharing && row.sshTarget.length > 0 && picker.currentIndex >= 0
            onClicked: forwarding.start(row.sshTarget, forwarding.inputs[picker.currentIndex].id, hostCamera.text)
        }
        Button { text: qsTr("Stop"); enabled: forwarding.sharing || forwarding.busy; onClicked: forwarding.stop() }
    }
    Label { width: parent.width; wrapMode: Text.WordWrap; text: forwarding.status }
}
