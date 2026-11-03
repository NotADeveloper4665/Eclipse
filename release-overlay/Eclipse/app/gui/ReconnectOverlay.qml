import QtQuick 2.9
import QtQuick.Controls 2.2

Item {
    id: reconnectOverlay
    objectName: "reconnectOverlay"

    property bool reconnecting: false
    property url snapshotUrl: ""
    signal snapshotLoaded()

    visible: reconnecting
    opacity: reconnecting ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 180 }
    }

    Rectangle {
        anchors.fill: parent
        color: "#11131B"
    }

    Image {
        id: reconnectFrame
        anchors.fill: parent
        source: reconnectOverlay.snapshotUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: false
        cache: false
        smooth: true
        visible: reconnectOverlay.snapshotUrl.toString().length > 0

        onStatusChanged: {
            if (status === Image.Ready && reconnectOverlay.reconnecting) {
                reconnectOverlay.snapshotLoaded()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#700D101B"
    }

    Column {
        anchors.centerIn: parent
        spacing: 14

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: reconnectOverlay.reconnecting
            visible: reconnectOverlay.reconnecting
            width: 42
            height: 42
        }

        Label {
            objectName: "reconnectStatusText"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Reconnecting")
            color: "#FFFFFF"
            font.pixelSize: 25
            font.weight: Font.DemiBold
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Restoring your stream connection")
            color: "#D5D0E2"
            font.pixelSize: 14
        }
    }
}