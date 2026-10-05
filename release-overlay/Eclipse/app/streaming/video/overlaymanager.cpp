#include "overlaymanager.h"
#include "path.h"
#include <QMutexLocker>
#include <QStringList>

using namespace Overlay;

QString OverlayManager::statisticsSnapshot()
{
    QMutexLocker lock(&m_StatisticsLock);
    return m_Statistics;
}

void OverlayManager::updateStatistics(const char* compactText, const char* detailedText)
{
    { QMutexLocker lock(&m_StatisticsLock); m_Statistics = QString::fromUtf8(detailedText ? detailedText : compactText); }
    m_CompactStatistics = true;
    updateOverlayText(OverlayDebug, compactText);
}

void OverlayManager::publishControlSurface(SDL_Surface* surface)
{
    SDL_Surface* old = static_cast<SDL_Surface*>(SDL_AtomicSetPtr(
        reinterpret_cast<void**>(&m_Overlays[OverlayControlCenter].surface), surface));
    SDL_FreeSurface(old);
    if (m_Renderer) m_Renderer->notifyOverlayUpdated(OverlayControlCenter);
}

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));

    m_Overlays[OverlayType::OverlayDebug].color = {0xD0, 0xD0, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = 20;

    m_Overlays[OverlayType::OverlayStatusUpdate].color = {0xCC, 0x00, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayStatusUpdate].fontSize = 36;

    // While TTF will usually not be initialized here, it is valid for that not to
    // be the case, since Session destruction is deferred and could overlap with
    // the lifetime of a new Session object.
    //SDL_assert(TTF_WasInit() == 0);

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }
}

OverlayManager::~OverlayManager()
{
    for (int i = 0; i < OverlayType::OverlayMax; i++) {
        if (m_Overlays[i].surface != nullptr) {
            SDL_FreeSurface(m_Overlays[i].surface);
        }
        if (m_Overlays[i].font != nullptr) {
            TTF_CloseFont(m_Overlays[i].font);
        }
    }

    TTF_Quit();

    // For similar reasons to the comment in the constructor, this will usually,
    // but not always, deinitialize TTF. In the cases where Session objects overlap
    // in lifetime, there may be an additional reference on TTF for the new Session
    // that means it will not be cleaned up here.
    //SDL_assert(TTF_WasInit() == 0);
}

bool OverlayManager::isOverlayEnabled(OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::updateOverlayText(OverlayType type, const char* text)
{
    SDL_utf8strlcpy(m_Overlays[type].text, text, sizeof(m_Overlays[0].text));
    setOverlayTextUpdated(type);
}

int OverlayManager::getOverlayMaxTextLength()
{
    return sizeof(m_Overlays[0].text);
}

int OverlayManager::getOverlayFontSize(OverlayType type)
{
    return m_Overlays[type].fontSize;
}

SDL_Surface* OverlayManager::getUpdatedOverlaySurface(OverlayType type)
{
    // If a new surface is available, return it. If not, return nullptr.
    // Caller must free the surface on success.
    return (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);
}

void OverlayManager::setOverlayTextUpdated(OverlayType type)
{
    // Only update the overlay state if it's enabled. If it's not enabled,
    // the renderer has already been notified by setOverlayState().
    if (m_Overlays[type].enabled) {
        notifyOverlayUpdated(type);
    }
}

void OverlayManager::setOverlayState(OverlayType type, bool enabled)
{
    bool stateChanged = m_Overlays[type].enabled != enabled;

    m_Overlays[type].enabled = enabled;

    if (stateChanged) {
        if (!enabled) {
            // Set the text to empty string on disable
            m_Overlays[type].text[0] = 0;
        }

        notifyOverlayUpdated(type);
    }
}

SDL_Color OverlayManager::getOverlayColor(OverlayType type)
{
    return m_Overlays[type].color;
}

void OverlayManager::setOverlayRenderer(IOverlayRenderer* renderer)
{
    m_Renderer = renderer;
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    if (m_Renderer == nullptr) {
        return;
    }

    if (type == OverlayControlCenter) {
        // Raster surface is supplied by the SDL event loop, rather than TTF.
        m_Renderer->notifyOverlayUpdated(type);
        return;
    }

    // Construct the required font to render the overlay
    if (m_Overlays[type].font == nullptr) {
        if (m_FontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // m_FontData must stay around until the font is closed
#ifdef Q_OS_WIN
        m_Overlays[type].font = TTF_OpenFont("C:/Windows/Fonts/segoeui.ttf", m_Overlays[type].fontSize);
#endif
        if (m_Overlays[type].font == nullptr) {
            m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
                                                   1,
                                                   m_Overlays[type].fontSize);
        }
        if (m_Overlays[type].font == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    // Exchange the old surface with the new one
    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr(
        (void**)&m_Overlays[type].surface,
        m_Overlays[type].enabled ?
            // The _Wrapped variant is required for line breaks to work
            (type == OverlayDebug && m_CompactStatistics) ?
                RenderCompactStatsCard(m_Overlays[type].font, m_Overlays[type].text) :
                RenderTextOutlinedWrapped(m_Overlays[type].font,
                                          m_Overlays[type].text,
                                          m_Overlays[type].color,
                                          {0, 0, 0, 255},
                                          4,
                                          1024)
            : nullptr);

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }
}

SDL_Surface* OverlayManager::RenderCompactStatsCard(TTF_Font* font, const char* text)
{
    const QStringList lines = QString::fromUtf8(text).split('\n');
    if (lines.isEmpty()) {
        return nullptr;
    }

    const int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, 0);
    QVector<SDL_Surface*> renderedLines;
    int textWidth = 0;
    int textHeight = 0;
    for (const QString& line : lines) {
        SDL_Surface* rendered = TTF_RenderUTF8_Blended(font, line.toUtf8().constData(), {238, 241, 246, 255});
        if (rendered == nullptr) {
            for (SDL_Surface* previous : renderedLines) SDL_FreeSurface(previous);
            TTF_SetFontOutline(font, oldOutline);
            return nullptr;
        }
        renderedLines.append(rendered);
        textWidth = qMax(textWidth, rendered->w);
        textHeight += qMax(TTF_FontLineSkip(font), rendered->h);
    }
    TTF_SetFontOutline(font, oldOutline);

    constexpr int horizontalPadding = 12;
    constexpr int verticalPadding = 8;
    constexpr int accentWidth = 4;
    const int cardWidth = textWidth + horizontalPadding * 2 + accentWidth;
    const int cardHeight = textHeight + verticalPadding * 2;
    SDL_Surface* card = SDL_CreateRGBSurfaceWithFormat(0, cardWidth, cardHeight, 32, SDL_PIXELFORMAT_RGBA32);
    if (card == nullptr) {
        for (SDL_Surface* rendered : renderedLines) SDL_FreeSurface(rendered);
        return nullptr;
    }

    const Uint32 background = SDL_MapRGBA(card->format, 22, 25, 31, 218);
    SDL_FillRect(card, nullptr, background);
    const int radius = 9;
    const Uint32 transparent = SDL_MapRGBA(card->format, 0, 0, 0, 0);
    const bool mustLock = SDL_MUSTLOCK(card);
    if (mustLock) SDL_LockSurface(card);
    auto* pixels = static_cast<Uint32*>(card->pixels);
    for (int y = 0; y < cardHeight; ++y) {
        for (int x = 0; x < cardWidth; ++x) {
            const int cornerX = x < radius ? radius - x : (x >= cardWidth - radius ? x - (cardWidth - radius - 1) : 0);
            const int cornerY = y < radius ? radius - y : (y >= cardHeight - radius ? y - (cardHeight - radius - 1) : 0);
            if (cornerX > 0 && cornerY > 0 && cornerX * cornerX + cornerY * cornerY > radius * radius) {
                pixels[y * card->pitch / 4 + x] = transparent;
            }
        }
    }
    if (mustLock) SDL_UnlockSurface(card);

    SDL_Rect accentRect{0, radius / 2, accentWidth, cardHeight - radius};
    SDL_FillRect(card, &accentRect, SDL_MapRGBA(card->format, 99, 181, 245, 255));
    SDL_SetSurfaceBlendMode(card, SDL_BLENDMODE_BLEND);

    int y = verticalPadding;
    for (SDL_Surface* rendered : renderedLines) {
        SDL_Rect destination{horizontalPadding + accentWidth, y, rendered->w, rendered->h};
        SDL_BlitSurface(rendered, nullptr, card, &destination);
        y += qMax(TTF_FontLineSkip(font), rendered->h);
        SDL_FreeSurface(rendered);
    }
    return card;
}

SDL_Surface* OverlayManager::RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth) {
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, outlineWidth);

    // Verify that the string won't require wrapping (which could cause the outline and the text
    // to diverge due to different wrapping positions).
    //
    // FIXME: We do this rather than just disabling wrapping entirely (wrapWidth = 0) because we
    // need further testing to ensure that all renderers can handle non-NPOT overlay textures.
    for (const QString& line : QString(text).split('\n')) {
        int extent, count;
        if (TTF_MeasureUTF8(font, line.toUtf8(), wrapWidth, &extent, &count) == 0 && count < line.size()) {
            // If it requires wrapping, render it without the outline
            TTF_SetFontOutline(font, oldOutline);
            return TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
        }
    }

    // Draw text twice, but outline is a bit bigger
    auto outlineSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, outlineColor, wrapWidth);
    TTF_SetFontOutline(font, 0);
    auto textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
    TTF_SetFontOutline(font, oldOutline);

    if (outlineSurface == nullptr || textSurface == nullptr) {
        SDL_FreeSurface(outlineSurface);
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    // Merge the texts
    SDL_Rect dst = { outlineWidth, outlineWidth, textSurface->w, textSurface->h };
    SDL_BlitSurface(textSurface, nullptr, outlineSurface, &dst);

    SDL_FreeSurface(textSurface);
    return outlineSurface;
}
