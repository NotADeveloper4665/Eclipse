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
    readonly property color lightSurfaceContainerLowest: "#FFFFFF"
    readonly property color lightSurfaceContainerLow: "#F7F3FA"
    readonly property color lightSurfaceContainer: "#F1EDF4"
    readonly property color lightSurfaceContainerHigh: "#ECE6F0"
    readonly property color lightSurfaceVariant: "#EAE7EF"
    readonly property color lightOnSurfaceVariant: "#62616A"
    readonly property color lightOutline: "#797680"
    readonly property color lightOutlineVariant: "#C9C5D0"
    readonly property color lightError: "#B3261E"

    readonly property color darkPrimary: "#D0BCFF"
    readonly property color darkOnPrimary: "#381E72"
    readonly property color darkPrimaryContainer: "#4F378B"
    readonly property color darkOnPrimaryContainer: "#EADDFF"
    readonly property color darkBackground: "#121318"
    readonly property color darkOnBackground: "#F1EFF7"
    readonly property color darkSurface: "#1E2028"
    readonly property color darkOnSurface: "#F1EFF7"
    readonly property color darkSurfaceContainerLowest: "#111218"
    readonly property color darkSurfaceContainerLow: "#1A1B22"
    readonly property color darkSurfaceContainer: "#20222A"
    readonly property color darkSurfaceContainerHigh: "#292B36"
    readonly property color darkSurfaceVariant: "#292B36"
    readonly property color darkOnSurfaceVariant: "#B8B4C5"
    readonly property color darkOutline: "#777383"
    readonly property color darkOutlineVariant: "#454351"
    readonly property color darkError: "#F2B8B5"
}