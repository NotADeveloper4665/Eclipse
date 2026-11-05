import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Window 2.2

// Consistent Material surface for a group of related preferences.
// Content and controls remain native Qt Quick Controls so keyboard and
// gamepad navigation semantics are preserved.
GroupBox {
    id: section
    padding: 16
    topPadding: 42
    bottomPadding: 16
    leftPadding: 16
    rightPadding: 16
    font.pointSize: 14

    label: Label {
        text: section.title
        leftPadding: 4
        color: Window.window.useDarkPalette ? Window.window.eclipseTheme.darkOnSurface : Window.window.eclipseTheme.lightOnSurface
        font.pixelSize: 16
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 260
        implicitHeight: 120
        radius: 16
        color: Window.window.useDarkPalette ? Window.window.eclipseTheme.darkSurfaceContainerLow : Window.window.eclipseTheme.lightSurfaceContainerLow
        border.width: 1
        border.color: Window.window.useDarkPalette ? Window.window.eclipseTheme.darkOutlineVariant : Window.window.eclipseTheme.lightOutlineVariant
    }
}