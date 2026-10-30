# Unified Material-Inspired UI Specification

Status: implementation baseline (M0)
Scope: Eclipse desktop client and Occultation Android client
Design decisions: light + dark themes, balanced density, shared design language

## Product direction

Use a conventional, restrained Material-inspired interface. Purple is the shared brand accent. Keep native platform behavior and accessibility; do not imitate a mobile UI on desktop or a desktop UI on phones.

Avoid neon/glow effects, glassmorphism, oversized decorative cards, gratuitous gradients, and decorative telemetry. Use surfaces, typography, spacing, shape, and elevation to establish hierarchy.

## Shared semantic color roles

These are initial seed values. Before UI rollout, validate text and control contrast in both themes and adjust values if needed.

| Role | Light | Dark |
|---|---|---|
| primary | #6750A4 | #D0BCFF |
| on-primary | #FFFFFF | #381E72 |
| primary-container | #EADDFF | #4F378B |
| on-primary-container | #21005D | #EADDFF |
| background | #F8F7FA | #121318 |
| on-background | #1B1B21 | #F1EFF7 |
| surface | #FFFFFF | #1E2028 |
| on-surface | #1B1B21 | #F1EFF7 |
| surface-variant | #EAE7EF | #292B36 |
| on-surface-variant | #62616A | #B8B4C5 |
| outline | #797680 | #454351 |
| error | #B3261E | #F2B8B5 |

Use semantic roles in components; avoid hard-coded color literals outside the theme definitions. Streaming video surfaces remain transparent/black where required for performance and correct compositing.

## Typography and density

- Use platform system typography with a consistent hierarchy: display/title, section title, body, label, and supporting text.
- Balanced density is the default: readable spacing, practical desktop information density, and touch targets that remain usable.
- Respect system font scaling and accessibility settings.
- Do not reduce touch target sizes solely to make desktop layouts denser.

## Shape, spacing, elevation

- Use a small, consistent radius scale; prefer modest rounding over pill-shaped everything.
- Use a spacing scale based on 4 dp/px increments, with 8/12/16/24 as common values.
- Reserve elevation for actual layering (dialogs, menus, floating stream controls). Prefer tonal surface differences over shadows for ordinary groups.
- Use borders sparingly and consistently.

## Shared component vocabulary

Equivalent controls in both apps should share names, semantics, states, and hierarchy:
- primary, secondary, and text buttons
- text fields and search fields
- switches, sliders, checkboxes, and segmented controls
- dialogs, menus, sheets, and tooltips
- navigation destinations
- host rows/cards and status indicators
- settings sections and supporting descriptions
- profile selectors
- stream control panels and telemetry rows
- loading, empty, offline, connecting, and error states

## Application navigation

### Eclipse
Desktop-first navigation with a persistent sidebar or navigation rail where window size permits. Preserve efficient keyboard, mouse, and gamepad navigation. Host discovery and the host list remain the primary landing experience.

### Occultation
Use Android-native navigation patterns. Phone layouts prioritize a clear top-level destination and host list; tablets and desktop-mode windows may use a navigation rail or two-pane layout. Preserve TV/controller navigation.

## Stream control surfaces

Use a compact, expandable control center. Group controls by Input, Video, Network, Performance, and More. Show only useful live metrics, with clear units and labels. The overlay must not intercept stream input when hidden and must preserve predictable focus and dismissal behavior.

## Theme behavior

- Provide explicit Light and Dark options, plus System default.
- Persist the user's selection.
- Theme changes must not affect streaming video output, decoder behavior, or overlay input routing.
- Use platform-native system bars and accessibility contrast behavior on Android.
- Eclipse must handle theme changes without rebuilding or losing the current navigation state.

## Cross-platform input behavior

The visual input-mode names and semantics must match: Native Touch, Absolute Mouse, Trackpad/Relative Mouse, Keyboard, Gamepad, and Disabled where supported. Do not silently substitute a different mode if a requested mode is unsupported. Clearly indicate capability limitations.

Touch coordinates must map through the actual rendered video rectangle, including aspect-ratio letterboxing and resize. Clear active contacts and held inputs on focus loss, stream teardown, and disconnect.

## Settings and profiles

Use consistent categories: Appearance, Input, Stream, Overlay, Performance, and Network. Keep platform-specific settings platform-specific. Global defaults and per-host overrides must not silently overwrite one another. Existing preferences require migration and regression tests.

## Accessibility and validation

- Verify text, icon, and control contrast in both themes.
- Support keyboard/gamepad focus and visible focus indication.
- Respect font scaling and window resizing.
- Test compact and wide layouts, touch targets, screen readers where practical, and reduced-motion preferences.
- Verify that theme changes do not alter streaming performance or input delivery.

## Platform implementation notes

### Eclipse (Qt Quick Controls / QML)
- Inspect the deployed Qt version and current style selection before choosing runtime Material style configuration.
- Centralize semantic roles and theme selection in reusable QML theme objects.
- Use Qt Material style where compatible with the packaged Qt runtime; provide custom QML components only where app-specific behavior requires them.
- Balanced density should not mean globally forcing Qt's Dense variant; use normal controls for touch-facing surfaces and compact layouts for desktop-only data panels.
- Preserve existing Moonlight keyboard/gamepad focus behavior and localization.

### Occultation (Android)
- Keep native Android components and existing Material Components/Material 3 dependencies.
- Centralize light/dark color resources and theme overlays. Audit base app, settings, stream, and API-level-specific themes independently.
- Do not assume a Material 3 theme on one API level means all resources or legacy widgets are using Material 3.
- Preserve TV, gamepad, mouse, external display, and multi-window behavior.

## Out of scope for the visual foundation

- Rewriting either app's navigation or streaming architecture from scratch.
- Changing Syzygy pairing, network protocol, or host authorization.
- Removing existing platform-specific features.
- Publishing a release or merging into a stable branch.

## Acceptance gates

1. Both apps expose Light, Dark, and System appearance modes.
2. Equivalent controls use matching semantic roles and interaction states.
3. Host discovery, pairing, profiles, streaming, and input behavior remain functional.
4. Theme changes preserve current navigation and do not interrupt an active stream.
5. Existing settings migrate without loss.
6. Desktop, phone, tablet, touchscreen laptop, and TV/controller navigation are validated.
