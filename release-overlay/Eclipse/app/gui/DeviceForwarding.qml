import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

Column {
    id: row
    property string title
    property string deviceKind: "all"
    property string sshTarget
    spacing: 6
    property var forwarding
    property var filteredDevices: {
        var choices = []
        var devices = forwarding.devices
        for (var i = 0; i < devices.length; ++i) {
            if (deviceKind === "all" || devices[i][deviceKind]) choices.push(devices[i])
        }
        return choices
    }
    Label { text: row.title; font.bold: true }
    ComboBox {
        id: picker
        width: parent.width
        enabled: forwarding.supported && !forwarding.busy && !forwarding.sharing && !forwarding.canStop
        model: row.filteredDevices
        textRole: "label"
    }
    Flow {
        spacing: 6
        width: parent.width
        Button {
            text: qsTr("Refresh")
            enabled: !forwarding.busy
            onClicked: forwarding.refresh()
        }
        Button {
            text: qsTr("Share")
            enabled: forwarding.supported && !forwarding.busy && !forwarding.sharing && !forwarding.canStop && row.sshTarget.length > 0 && picker.currentIndex >= 0
            onClicked: forwarding.share(row.sshTarget, row.filteredDevices[picker.currentIndex].busId)
        }
        Button {
            text: qsTr("Stop sharing")
            enabled: forwarding.canStop
            onClicked: forwarding.stop()
        }
    }
    Label {
        width: parent.width
        text: forwarding.status
        wrapMode: Text.WordWrap
    }
}
