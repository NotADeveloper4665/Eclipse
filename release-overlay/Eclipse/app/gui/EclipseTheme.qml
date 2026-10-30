import QtQuick 2.9

// Shared semantic palette for Eclipse. Keep role names aligned with
// Occultation's Android resources; components should consume roles rather
// than introducing screen-specific color literals.
QtObject {
    readonly property color lightPrimary: "#6750A4"
    readonly property color lightOnPrimary: "#FFFFFF"
    readonly property color lightPrimaryContainer: "#EADDFF"
    readonly property color lightOnPrimaryContainer: "#21005D"
    readonly property color lightBackground: "#F8F7FA"
    readonly property color lightOnBackground: "#1B1B21"
    readonly property color lightSurface: "#FFFFFF"
    readonly property color lightOnSurface: "#1B1B21"
    readonly property color lightSurfaceVariant: "#EAE7EF"
    readonly property color lightOnSurfaceVariant: "#62616A"
    readonly property color lightOutline: "#797680"

    readonly property color darkPrimary: "#D0BCFF"
    readonly property color darkOnPrimary: "#381E72"
    readonly property color darkPrimaryContainer: "#4F378B"
    readonly property color darkOnPrimaryContainer: "#EADDFF"
    readonly property color darkBackground: "#121318"
    readonly property color darkOnBackground: "#F1EFF7"
    readonly property color darkSurface: "#1E2028"
    readonly property color darkOnSurface: "#F1EFF7"
    readonly property color darkSurfaceVariant: "#292B36"
    readonly property color darkOnSurfaceVariant: "#B8B4C5"
    readonly property color darkOutline: "#454351"
}
