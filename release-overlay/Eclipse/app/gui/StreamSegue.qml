import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Window 2.2

import SdlGamepadKeyNavigation 1.0
import Session 1.0
import SystemProperties 1.0

Item {
    id: streamSegue
    property Session session
    property var reconnectSession: null
    property string reconnectSnapshotUrl: ""
    property bool reconnecting: false
    property string appName
    property string stageText : isResume ? qsTr("Resuming %1...").arg(appName) :
                                           qsTr("Starting %1...").arg(appName)
    property bool isResume : false
    property bool quitAfter : false

    function stageStarting(stage)
    {
        // Update the spinner text
        stageText = qsTr("Starting %1...").arg(stage)
    }

    function stageFailed(stage, errorCode, failingPorts)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = qsTr("Starting %1 failed: Error %2").arg(stage).arg(errorCode)

        if (failingPorts) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("Check your firewall and port forwarding rules for port(s): %1").arg(failingPorts)
        }
    }

    function connectSessionSignals(targetSession)
    {
        targetSession.stageStarting.connect(stageStarting)
        targetSession.stageFailed.connect(stageFailed)
        targetSession.connectionStarted.connect(connectionStarted)
        targetSession.displayLaunchError.connect(displayLaunchError)
        targetSession.quitStarting.connect(quitStarting)
        targetSession.sessionFinished.connect(sessionFinished)
        targetSession.readyForDeletion.connect(sessionReadyForDeletion)
    }

    function releaseReconnectSnapshotFile()
    {
        // The image has already been decoded into memory. Delete the temporary
        // PNG without clearing its URL, so the frozen frame remains on screen.
        if (reconnectSnapshotUrl.toString().length > 0 && session) {
            session.clearReconnectSnapshotFile(reconnectSnapshotUrl.toString())
        }
    }

    function clearReconnectSnapshot()
    {
        releaseReconnectSnapshotFile()
        reconnectSnapshotUrl = ""
    }

    function connectionStarted()
    {
        // The replacement video window is ready. Remove the frozen frame and
        // hand the display back to the live SDL stream without popping the page.
        clearReconnectSnapshot()
        reconnecting = false

        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        stageSpinner.visible = false
        stageLabel.visible = false
        hintText.visible = false

        // Hide the window now that streaming has begun
        window.visible = false
    }

    function displayLaunchError(text)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = text
        console.error(text)
    }

    function quitStarting()
    {
        // Avoid the push transition animation
        var component = Qt.createComponent("QuitSegue.qml")
        stackView.replace(stackView.currentItem, component.createObject(stackView, {"appName": appName}), StackView.Immediate)

        // Show the Qt window again to show quit segue
        window.visible = true
    }

    function sessionFinished(portTestResult)
    {
        // Restart was requested from the in-stream control center. Preserve the
        // current segue and its frozen frame while a replacement Session starts.
        if (session && session.reconnectRequested) {
            reconnectSession = session.createReconnectSession()
            if (reconnectSession) {
                reconnecting = true
                reconnectSnapshotUrl = session.reconnectSnapshotUrl
                streamSegueErrorDialog.text = ""
                stageSpinner.visible = false
                stageLabel.visible = false
                hintText.visible = false
                window.visible = true
                return
            }
            displayLaunchError(qsTr("Unable to prepare the replacement stream session."))
        }
        else if (reconnecting) {
            // The replacement session ended before streaming resumed.
            clearReconnectSnapshot()
            reconnecting = false
            reconnectSession = null
        }

        if (portTestResult !== 0 && portTestResult !== -1 && streamSegueErrorDialog.text) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
        }

        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()

        // Pop the StreamSegue off the stack if this is a GUI-based app launch
        if (!quitAfter) {
            stackView.pop()
        }

        if (quitAfter && !streamSegueErrorDialog.text) {
            // If this was a CLI launch without errors, exit now
            Qt.quit()
        }
        else {
            // Show the Qt window again after streaming
            window.visible = true

            // Display any launch errors. We do this after
            // the Qt UI is visible again to prevent losing
            // focus on the dialog which would impact gamepad
            // users.
            if (streamSegueErrorDialog.text) {
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
        }
    }

    function sessionReadyForDeletion()
    {
        // A reconnect creates a fresh Session only after the previous one has
        // released Limelight, SDL, and its transport resources.
        if (reconnecting && reconnectSession) {
            session = null
            gc()
            session = reconnectSession
            reconnectSession = null
            connectSessionSignals(session)
            // Let the frozen frame paint before decoder probing can block the UI.
            reconnectStartTimer.start()
            return
        }

        session = null
        gc()
    }

    StackView.onDeactivating: {
        // Show the toolbar again when popped off the stack
        toolBar.visible = true

        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()
    }

    StackView.onActivated: {
        // Hide the toolbar before we start loading
        toolBar.visible = false

        // Hook up our signals
        connectSessionSignals(session)

        // Ensure the SystemProperties async thread is finished,
        // since it may currently be using the SDL video subsystem
        SystemProperties.waitForAsyncLoad()

        // Kick off the stream
        streamLoader.active = true
    }

    Timer {
        id: reconnectStartTimer
        interval: 16
        repeat: false
        onTriggered: {
            if (!session.initialize(window)) {
                reconnecting = false
                clearReconnectSnapshot()
                sessionFinished(0)
                sessionReadyForDeletion()
                return
            }
            session.start()
        }
    }

    Timer {
        id: startSessionTimer
        onTriggered: {
            // Garbage collect QML stuff before we start streaming,
            // since we'll probably be streaming for a while and we
            // won't be able to GC during the stream.
            gc()

            // Run the streaming session to completion
            session.start()
        }
    }

    Loader {
        id: streamLoader
        active: false
        asynchronous: true

        onLoaded: {
            // Set the hint text. We do this here rather than
            // in the hintText control itself to synchronize
            // with Session.exec() which requires no concurrent
            // gamepad usage.
            hintText.text = qsTr("Tip:") + " " + qsTr("Press %1 to disconnect your session").arg(SdlGamepadKeyNavigation.getConnectedGamepads() > 0 ?
                                                  qsTr("Start+Select+L1+R1") : qsTr("Ctrl+Alt+Shift+Q"))

            // Stop GUI gamepad usage now
            SdlGamepadKeyNavigation.disable()

            // Initialize the session and probe for host/client capabilities
            if (!session.initialize(window)) {
                sessionFinished(0);
                sessionReadyForDeletion();
                return;
            }

            // This spinner is shown only after session.initialize() has completed
            // to prevent active animations from running during decoder probing,
            // which causes re-entrant event loop livelocks with libdecor-gtk.
            stageSpinner.visible = true

            // Don't wait unless we have toasts to display
            startSessionTimer.interval = 0

            // Display the toasts together in a vertical centered arrangement
            var yOffset = 0
            for (var i = 0; i < session.launchWarnings.length; i++) {
                var text = session.launchWarnings[i]
                console.warn(text)

                // Show the tooltip for 3 seconds
                var toast = Qt.createQmlObject('import QtQuick.Controls 2.2; ToolTip {}', parent, '')
                toast.timeout = 3000
                toast.text = text
                toast.y += yOffset
                toast.visible = true

                // Offset the next toast below the previous one
                yOffset = toast.y + toast.padding + toast.height

                // Allow an extra 500 ms for the tooltip's fade-out animation to finish
                startSessionTimer.interval = toast.timeout + 500;
            }

            // Start the timer to wait for toasts (or start the session immediately)
            startSessionTimer.start()
        }

        sourceComponent: Item {}
    }


    // Keep the last captured frame visible until the replacement stream is live.
    ReconnectOverlay {
        id: reconnectOverlay
        anchors.fill: parent
        z: 1000
        reconnecting: streamSegue.reconnecting
        snapshotUrl: streamSegue.reconnectSnapshotUrl
        onSnapshotLoaded: streamSegue.releaseReconnectSnapshotFile()
    }

    Row {
        anchors.centerIn: parent
        spacing: 5

        BusyIndicator {
            id: stageSpinner
            running: visible
            visible: false
        }

        Label {
            id: stageLabel
            height: stageSpinner.height
            text: stageText
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter

            wrapMode: Text.Wrap
        }
    }

    Label {
        id: hintText
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        font.pointSize: 18
        verticalAlignment: Text.AlignVCenter

        wrapMode: Text.Wrap
    }
}
