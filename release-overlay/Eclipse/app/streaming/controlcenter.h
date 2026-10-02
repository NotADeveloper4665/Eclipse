#pragma once

#include <QImage>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include "SDL_compat.h"
#include "settings/streamingpreferences.h"

// Rasterized into the stream's overlay plane: no second window or Qt event loop.
class ControlCenter
{
public:
    enum Action { Close, Fullscreen, ReleaseMouse, Mute, Stats, Restart, Disconnect, SecureAttention };
    explicit ControlCenter(StreamingPreferences* preferences);
    QImage render(QSize pixels, bool muted, bool fullscreen, const QString& statistics);
    bool handleEvent(const SDL_Event& event, QSize windowSize);
    void save();
    std::function<void(Action)> action;
    bool dirty() const { return m_Dirty; }

private:
    enum Dropdown { NoDropdown, ResolutionDropdown, FrameRateDropdown, CodecDropdown, AudioDropdown };
    struct Target { QRectF rect; QString label; std::function<void(int)> invoke; };
    void activate(int index, int direction = 1);
    StreamingPreferences* m_Preferences;
    QVector<Target> m_Targets;
    QSize m_Pixels;
    qreal m_Scale = 1;
    QPointF m_Origin;
    int m_Page = 0;
    int m_Focus = -1;
    bool m_Dirty = false;
    bool m_ConfirmDisconnect = false;
    int m_Width, m_Height, m_Fps, m_Bitrate;
    StreamingPreferences::VideoCodecConfig m_Codec;
    StreamingPreferences::AudioConfig m_Audio;
    bool m_Hdr, m_Vsync, m_Pacing, m_AbsoluteMouse, m_MultiController;
    int m_OpenDropdown = NoDropdown;
    int m_DropdownSelection = 0;
    QStringList m_DropdownItems;
    QVector<QRectF> m_DropdownOptionRects;
    QRectF m_DropdownSelectorRect;
    QRectF m_DropdownPopupRect;
    std::function<void(int)> m_DropdownApply;
    QVector<double> m_History[5];
    QString m_LastStatistics;
};
