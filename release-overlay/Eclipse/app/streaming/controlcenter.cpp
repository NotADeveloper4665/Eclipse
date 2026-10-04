#include "controlcenter.h"
#include <QPainter>
#include <QFont>
#include <QRegularExpression>
#include <algorithm>

ControlCenter::ControlCenter(StreamingPreferences* p) :
    m_Preferences(p), m_Width(p->width), m_Height(p->height), m_Fps(p->fps),
    m_Bitrate(p->bitrateKbps), m_Codec(p->videoCodecConfig), m_Audio(p->audioConfig),
    m_Hdr(p->enableHdr), m_Vsync(p->enableVsync), m_Pacing(p->framePacing),
    m_AbsoluteMouse(p->absoluteMouseMode), m_MultiController(p->multiController)
{}

void ControlCenter::save()
{
    auto* p = m_Preferences;
    p->width = m_Width; p->height = m_Height; p->fps = m_Fps;
    p->bitrateKbps = m_Bitrate; p->videoCodecConfig = m_Codec;
    p->audioConfig = m_Audio; p->enableHdr = m_Hdr;
    p->enableVsync = m_Vsync; p->framePacing = m_Pacing;
    p->absoluteMouseMode = m_AbsoluteMouse; p->multiController = m_MultiController;
    p->save();
    emit p->displayModeChanged();
    emit p->bitrateChanged();
    emit p->videoCodecConfigChanged();
    emit p->audioConfigChanged();
    emit p->enableHdrChanged();
    emit p->enableVsyncChanged();
    emit p->framePacingChanged();
    emit p->absoluteMouseModeChanged();
    emit p->multiControllerChanged();
    m_Dirty = false;
}

void ControlCenter::setBitrateFromPoint(const QPointF& point)
{
    if (m_BitrateSliderRect.width() <= 0) return;
    const qreal ratio = std::max<qreal>(0,std::min<qreal>(1,
        (point.x()-(m_BitrateSliderRect.left()+1))/(m_BitrateSliderRect.width()-2)));
    m_Bitrate = std::max(1000,std::min(150000,static_cast<int>(std::lround(1000+ratio*149000))));
    m_Dirty = true;
}

QImage ControlCenter::render(QSize pixels, bool muted, bool fullscreen, const QString& statistics)
{
    m_Pixels = pixels;
    QImage image(pixels, QImage::Format_RGBA8888);
    image.fill(QColor(0, 0, 0, 95));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    m_Scale = std::min({qreal(1.25), pixels.width() / 1120.0, pixels.height() / 690.0});
    m_Origin = QPointF((pixels.width() - 1080 * m_Scale) / 2,
                      (pixels.height() - 640 * m_Scale) / 2);
    painter.translate(m_Origin);
    painter.scale(m_Scale, m_Scale);
    auto text = [&](QRectF r, QString value, int size = 14, QColor color = QColor("#eeeef6")) {
        QFont font; font.setPixelSize(size);
        painter.setFont(font); painter.setPen(color);
        painter.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, value);
    };
    auto card = [&](QRectF r, QColor color) {
        painter.setPen(Qt::NoPen); painter.setBrush(color);
        painter.drawRoundedRect(r, 4, 4);
    };
    m_Targets.clear();
    m_DropdownOptionRects.clear();
    m_BitrateSliderRect = QRectF();
    auto button = [&](QRectF r, QString label, std::function<void(int)> fn, bool selected = false) {
        int index = m_Targets.size();
        m_Targets.append({r, label, std::move(fn)});
        card(r, QColor("#424242"));
        if (selected) {
            painter.setPen(Qt::NoPen); painter.setBrush(QColor("#9FA8DA"));
            painter.drawRoundedRect(QRectF(r.x(),r.y()+5,3,r.height()-10),2,2);
        }
        if (index == m_Focus && m_Focus >= 0) {
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor("#9FA8DA"), 2));
            painter.drawRoundedRect(r.adjusted(1,1,-1,-1), 4, 4);
        }
        text(r.adjusted(14,0,-8,0), label, 13);
    };
    // Moonlight uses Qt Quick Controls Material dark surfaces: #303030 canvas, #424242 surfaces.
    // Moonlight's dark theme uses an indigo-purple accent.
    card({0,0,1080,640}, QColor("#303030"));
    card({0,0,205,640}, QColor("#363636"));
    text({24,20,170,35}, "Eclipse", 24);
    text({24,56,170,24}, "QUICK MENU", 11, QColor("#BDBDBD"));
    const QStringList pages = {"General", "Video & Display", "Audio", "Controllers & Input", "Network", "Help"};
    for (int i = 0; i < pages.size(); ++i) {
        button({14,108.0 + i*52,177,42}, pages[i], [this,i](int) {
            m_Page = i; m_Focus = i; m_ConfirmDisconnect = false;
        }, i == m_Page);
    }
    text({24,555,172,25}, "Sunshine compatible", 11, QColor("#BDBDBD"));
    text({24,582,175,25}, "Alt + Super + O", 12, QColor("#9FA8DA"));
    button({227,20,126,38}, fullscreen ? "Windowed" : "Fullscreen", [this](int){ action(Fullscreen); });
    button({363,20,141,38}, "Release cursor", [this](int){ action(ReleaseMouse); });
    button({514,20,123,38}, muted ? "Unmute audio" : "Mute audio", [this](int){ action(Mute); });
    button({647,20,105,38}, "Reconnect", [this](int){ save(); action(Restart); });
    button({1017,20,42,38}, "X", [this](int){ action(Close); });
    text({233,82,525,42}, pages[m_Page], 25);
    text({233,124,525,28}, "Your stream. Your settings.", 13, QColor("#BDBDBD"));

    int row = 0;
    auto setting = [&](QString label, QString value, std::function<void(int)> fn) {
        qreal y = 171 + row++ * 61;
        card({229,y,523,52}, QColor("#424242"));
        text({245,y,240,52}, label, 14);
        button({492,y+7,245,38}, value, [this,fn](int direction){ fn(direction); m_Dirty = true; });
    };
    auto dropdownSetting = [&](QString label, QString value, int dropdown, QStringList options,
                               int selected, std::function<void(int)> apply) {
        qreal y = 171 + row++ * 61;
        card({229,y,523,52}, QColor("#424242"));
        text({245,y,240,52}, label, 14);
        QRectF selector(492,y+7,245,38);
        int index = m_Targets.size();
        m_Targets.append({selector,label,[this,dropdown,options,selected,apply](int){
            m_OpenDropdown = dropdown;
            m_DropdownItems = options;
            m_DropdownSelection = selected;
            m_DropdownApply = apply;
        }});
        card(selector,QColor("#424242"));
        if (index == m_Focus && m_Focus >= 0) {
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor("#9FA8DA"),2));
            painter.drawRoundedRect(selector.adjusted(1,1,-1,-1),4,4);
        }
        text(selector.adjusted(14,0,-38,0),value,13);
        painter.setPen(Qt::NoPen); painter.setBrush(QColor("#BDBDBD"));
        QPolygonF caret; caret << QPointF(selector.right()-22,selector.center().y()-2)
                              << QPointF(selector.right()-12,selector.center().y()-2)
                              << QPointF(selector.right()-17,selector.center().y()+4);
        painter.drawPolygon(caret);

        if (m_OpenDropdown == dropdown) {
            const qreal optionHeight = 34;
            m_DropdownPopupRect = QRectF(selector.x(),y+45,selector.width(),options.size()*optionHeight);
            m_DropdownSelectorRect = selector;
            for (int i=0;i<m_DropdownItems.size();++i) {
                m_DropdownOptionRects.append(QRectF(m_DropdownPopupRect.x(),m_DropdownPopupRect.y()+i*optionHeight,
                                                    m_DropdownPopupRect.width(),optionHeight));
            }
        }
    };
    auto yes = [](bool value){ return value ? QString("Enabled") : QString("Disabled"); };
    if (m_Page == 0) {
        card({229,171,523,118}, QColor("#424242"));
        text({247,181,490,34}, "Connected to your remote PC", 18);
        text({247,222,485,52}, "Close this menu to continue. Settings are saved\nwhen you choose Save or Reconnect.", 13, QColor("#BDBDBD"));
        button({229,310,251,44}, "Performance HUD", [this](int){ action(Stats); });
        button({495,310,257,44}, "Send Ctrl + Alt + Del", [this](int){ action(SecureAttention); });
        button({229,374,523,44}, m_ConfirmDisconnect ? "Confirm disconnect" : "Disconnect from this PC", [this](int){
            if (m_ConfirmDisconnect) action(Disconnect); else m_ConfirmDisconnect = true;
        });
        text({237,438,520,46}, "Disconnecting leaves your remote applications running.", 12, QColor("#BDBDBD"));
    }
    else if (m_Page == 1) {
        const QVector<int> resolutions{1280,1920,2560,3840};
        QStringList resolutionOptions{"1280 x 720","1920 x 1080","2560 x 1440","3840 x 2160"};
        dropdownSetting("Resolution", QString("%1 x %2").arg(m_Width).arg(m_Height), ResolutionDropdown,
                        resolutionOptions,resolutions.indexOf(m_Width),[this,resolutions](int index){
            m_Width = resolutions.value(index,1920);
            m_Height = m_Width * 9 / 16;
        });
        const QVector<int> frameRates{30,60,90,120,144};
        QStringList frameRateOptions{"30 FPS","60 FPS","90 FPS","120 FPS","144 FPS"};
        dropdownSetting("Frame rate",QString("%1 FPS").arg(m_Fps),FrameRateDropdown,
                        frameRateOptions,frameRates.indexOf(m_Fps),[this,frameRates](int index){
            m_Fps = frameRates.value(index,60);
        });
        const QVector<int> codecs{StreamingPreferences::VCC_AUTO,StreamingPreferences::VCC_FORCE_H264,
                                  StreamingPreferences::VCC_FORCE_HEVC,StreamingPreferences::VCC_FORCE_AV1};
        QStringList codecOptions{"Automatic","H.264","HEVC","AV1"};
        QString codecLabel = codecOptions.value(codecs.indexOf(static_cast<int>(m_Codec)),"Automatic");
        dropdownSetting("Video codec",codecLabel,CodecDropdown,codecOptions,
                        codecs.indexOf(static_cast<int>(m_Codec)),[this,codecs](int index){
            m_Codec = static_cast<StreamingPreferences::VideoCodecConfig>(codecs.value(index,StreamingPreferences::VCC_AUTO));
        });
        setting("HDR", yes(m_Hdr), [this](int){ m_Hdr = !m_Hdr; });
        setting("V-Sync", yes(m_Vsync), [this](int){ m_Vsync = !m_Vsync; });
        setting("Frame pacing", yes(m_Pacing), [this](int){ m_Pacing = !m_Pacing; });
    }
    else if (m_Page == 2) {
        button({229,171,523,48}, muted ? "Unmute stream audio" : "Mute stream audio", [this](int){ action(Mute); });
        row = 1;
        QStringList audioOptions{"Stereo","5.1 surround","7.1 surround"};
        dropdownSetting("Audio channels",audioOptions.value(static_cast<int>(m_Audio)),AudioDropdown,
                        audioOptions,static_cast<int>(m_Audio),[this](int index){
            m_Audio = static_cast<StreamingPreferences::AudioConfig>(index);
        });
        text({239,324,500,80}, "Mute applies immediately. Channel changes require\na reconnect. Microphone forwarding is unavailable\nin the Moonlight protocol.", 13, QColor("#BDBDBD"));
    }
    else if (m_Page == 3) {
        setting("Absolute mouse", yes(m_AbsoluteMouse), [this](int){ m_AbsoluteMouse = !m_AbsoluteMouse; });
        setting("Multiple controllers", yes(m_MultiController), [this](int){ m_MultiController = !m_MultiController; });
        text({239,310,500,100}, "Menu: hold Back, then press Start.\nNavigate: D-pad / Tab. Select: A / Enter.\nClose: B / Escape. Change value: Left / Right.\nUSB forwarding is unavailable in this client.", 13, QColor("#BDBDBD"));
    }
    else if (m_Page == 4) {
        card({229,171,523,52}, QColor("#424242"));
        text({245,171,240,52}, "Maximum video bitrate", 14);
        text({492,171,245,52}, QString("%1 Mbps").arg(m_Bitrate / 1000.0,0,'f',1), 14);

        m_BitrateSliderRect = QRectF(238,238,502,40);
        const qreal bitrateRatio = (m_Bitrate - 1000) / 149000.0;
        const QRectF bitrateTrack(239,253,500,6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#555555"));
        painter.drawRoundedRect(bitrateTrack,3,3);
        painter.setBrush(QColor("#9FA8DA"));
        painter.drawRoundedRect(QRectF(bitrateTrack.x(),bitrateTrack.y(),bitrateTrack.width()*bitrateRatio,bitrateTrack.height()),3,3);
        painter.drawEllipse(QPointF(bitrateTrack.x()+bitrateTrack.width()*bitrateRatio,bitrateTrack.center().y()),9,9);
        const int sliderIndex = m_Targets.size();
        m_Targets.append({m_BitrateSliderRect,"Maximum video bitrate",[this](int direction){
            m_Bitrate = std::max(1000,std::min(150000,m_Bitrate+(direction < 0 ? -5000 : 5000)));
            m_Dirty = true;
        }});
        if (sliderIndex == m_Focus && m_Focus >= 0) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor("#9FA8DA"),2));
            painter.drawRoundedRect(m_BitrateSliderRect.adjusted(1,1,-1,-1),5,5);
        }
        text({239,289,500,74}, "1 - 150 Mbps. Drag the slider or use Left/Right.\\nReconnect to apply the new limit.", 13, QColor("#BDBDBD"));
    }
    else {
        text({239,170,500,260}, "OPEN QUICK MENU\nAlt + Super / Command + O\nCtrl + Alt + Shift + O\nController: hold Back, press Start\n\nNAVIGATION\nTab / Shift + Tab or D-pad\nEnter / A selects; arrows change values\nEscape / B returns to your stream", 14);
    }
    if (m_OpenDropdown != NoDropdown) {
        card(m_DropdownPopupRect,QColor("#505050"));
        for (int i=0;i<m_DropdownOptionRects.size();++i) {
            const QRectF optionRect=m_DropdownOptionRects[i];
            if (i == m_DropdownSelection) {
                painter.setPen(Qt::NoPen); painter.setBrush(QColor("#9FA8DA"));
                painter.drawRoundedRect(QRectF(optionRect.x()+3,optionRect.y()+3,3,optionRect.height()-6),2,2);
                painter.setPen(QPen(QColor("#9FA8DA"),1)); painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(optionRect.adjusted(1,1,-1,-1),3,3);
            }
            text(optionRect.adjusted(13,0,-8,0),m_DropdownItems.value(i),13);
        }
    }
    button({229,583,241,38}, m_Dirty ? "Save changes" : "Settings saved", [this](int){ save(); });
    text({486,582,276,39}, "Stream changes need reconnect", 11, QColor("#BDBDBD"));

    card({779,82,281,538}, QColor("#383838"));
    text({798,95,245,35}, "Usage stats", 18);
    text({798,134,245,27}, "LIVE STREAM TELEMETRY", 10, QColor("#BDBDBD"));
    int metricIndex = 0;
    const bool newStatistics = statistics != m_LastStatistics;
    auto metric = [&](int y, QString label, QString pattern, QString suffix) {
        auto match = QRegularExpression(pattern).match(statistics);
        QString value = match.hasMatch() ? match.captured(1) + suffix : "Waiting for data";
        text({798,qreal(y),245,24}, label, 12, QColor("#BDBDBD"));
        text({798,qreal(y+26),245,37}, value, 20, QColor("#9FA8DA"));
        auto& history = m_History[metricIndex++];
        if (newStatistics && match.hasMatch()) {
            history.append(match.captured(1).toDouble());
            if (history.size() > 40) history.removeFirst();
        }
        if (history.size() > 1) {
            const double peak = std::max(1.0,*std::max_element(history.cbegin(),history.cend()));
            QPolygonF points;
            for (int i=0; i<history.size(); ++i) {
                points.append({949 + 91.0*i/(history.size()-1), y+60.0 - 24*history[i]/peak});
            }
            painter.setPen(QPen(QColor("#9FA8DA"),1.5));
            painter.drawPolyline(points);
        }
        painter.setPen(QColor("#383846")); painter.drawLine(798,y+74,1041,y+74);
    };
    metric(174,"Frame rate","Rendering frame rate: ([0-9.]+)"," FPS");
    metric(258,"Network latency","Average network latency: ([0-9]+)"," ms");
    metric(342,"Decode time","Average decoding time: ([0-9.]+)"," ms");
    metric(426,"Network frame loss","Frames dropped by your network connection: ([0-9.]+)"," %");
    metric(510,"Bandwidth","Bitrate: ([0-9.]+)"," Mbps");
    text({798,589,245,22}, "Frame loss is not packet loss.", 10, QColor("#BDBDBD"));
    m_LastStatistics = statistics;
    if (m_Focus >= m_Targets.size()) m_Focus = -1;
    return image;
}

void ControlCenter::activate(int index, int direction)
{
    if (index >= 0 && index < m_Targets.size()) {
        m_Focus = index;
        // Copy the callback: it may close the center or invalidate its targets.
        auto invoke = m_Targets[index].invoke;
        invoke(direction);
    }
}

bool ControlCenter::handleEvent(const SDL_Event& e, QSize windowSize)
{
    if (e.type == SDL_KEYDOWN) {
        if (e.key.repeat) return true;
        if (m_OpenDropdown != NoDropdown) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                m_OpenDropdown = NoDropdown; m_DropdownApply = {}; break;
            case SDLK_UP:
            case SDLK_LEFT:
                m_DropdownSelection = (m_DropdownSelection + m_DropdownItems.size() - 1) % std::max(1,m_DropdownItems.size()); break;
            case SDLK_DOWN:
            case SDLK_RIGHT:
                m_DropdownSelection = (m_DropdownSelection + 1) % std::max(1,m_DropdownItems.size()); break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                if (m_DropdownApply) m_DropdownApply(m_DropdownSelection);
                m_Dirty = true; m_OpenDropdown = NoDropdown; m_DropdownApply = {}; break;
            case SDLK_TAB:
                m_OpenDropdown = NoDropdown; m_DropdownApply = {};
                m_Focus = (m_Focus + m_Targets.size() + ((e.key.keysym.mod & KMOD_SHIFT) ? -1 : 1)) % std::max(1,int(m_Targets.size()));
                break;
            default: break;
            }
            return true;
        }
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE: action(Close); break;
        case SDLK_TAB: m_Focus = (m_Focus + m_Targets.size() + ((e.key.keysym.mod & KMOD_SHIFT) ? -1 : 1)) % std::max(1, int(m_Targets.size())); break;
        case SDLK_UP: m_Focus = (m_Focus + m_Targets.size() - 1) % std::max(1, int(m_Targets.size())); break;
        case SDLK_DOWN: m_Focus = (m_Focus + 1) % std::max(1, int(m_Targets.size())); break;
        case SDLK_LEFT: activate(m_Focus,-1); break;
        case SDLK_RIGHT: case SDLK_RETURN: case SDLK_SPACE: activate(m_Focus); break;
        default: break;
        }
        return true;
    }
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        if (m_OpenDropdown != NoDropdown) {
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_B:
                m_OpenDropdown = NoDropdown; m_DropdownApply = {}; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                m_DropdownSelection = (m_DropdownSelection + m_DropdownItems.size() - 1) % std::max(1,m_DropdownItems.size()); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                m_DropdownSelection = (m_DropdownSelection + 1) % std::max(1,m_DropdownItems.size()); break;
            case SDL_CONTROLLER_BUTTON_A:
                if (m_DropdownApply) m_DropdownApply(m_DropdownSelection);
                m_Dirty = true; m_OpenDropdown = NoDropdown; m_DropdownApply = {}; break;
            default: break;
            }
            return true;
        }
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_B: action(Close); break;
        case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: activate(m_Focus); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: activate(m_Focus,-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: m_Focus = (m_Focus+1)%std::max(1, int(m_Targets.size())); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: m_Focus = (m_Focus+m_Targets.size()-1)%std::max(1, int(m_Targets.size())); break;
        default: break;
        }
        return true;
    }
    auto mapMousePoint = [&](int x, int y) {
        QPointF pixelPoint(x * qreal(m_Pixels.width()) / std::max(1,windowSize.width()),
                           y * qreal(m_Pixels.height()) / std::max(1,windowSize.height()));
        return (pixelPoint - m_Origin) / m_Scale;
    };
    QPointF point;
    bool click = false;
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        point = mapMousePoint(e.button.x,e.button.y);
        click = true;
    }
    else if (e.type == SDL_FINGERDOWN) {
        point = (QPointF(e.tfinger.x*m_Pixels.width(),e.tfinger.y*m_Pixels.height())-m_Origin)/m_Scale;
        click = true;
    }
    else if (e.type == SDL_MOUSEMOTION && m_DraggingBitrate) {
        setBitrateFromPoint(mapMousePoint(e.motion.x,e.motion.y));
    }
    else if (e.type == SDL_MOUSEBUTTONUP && m_DraggingBitrate) {
        setBitrateFromPoint(mapMousePoint(e.button.x,e.button.y));
        m_DraggingBitrate = false;
    }
    else if (e.type == SDL_FINGERMOTION && m_DraggingBitrate) {
        setBitrateFromPoint((QPointF(e.tfinger.x*m_Pixels.width(),e.tfinger.y*m_Pixels.height())-m_Origin)/m_Scale);
    }
    else if (e.type == SDL_FINGERUP && m_DraggingBitrate) {
        setBitrateFromPoint((QPointF(e.tfinger.x*m_Pixels.width(),e.tfinger.y*m_Pixels.height())-m_Origin)/m_Scale);
        m_DraggingBitrate = false;
    }
    if (click) {
        if (m_BitrateSliderRect.contains(point)) {
            m_DraggingBitrate = true;
            setBitrateFromPoint(point);
            return true;
        }
        if (m_OpenDropdown != NoDropdown) {
            if (m_DropdownSelectorRect.contains(point)) {
                m_OpenDropdown = NoDropdown; m_DropdownApply = {};
                return true;
            }
            for (int i=0; i<m_DropdownOptionRects.size(); ++i) {
                if (m_DropdownOptionRects[i].contains(point)) {
                    if (m_DropdownApply) m_DropdownApply(i);
                    m_Dirty = true; m_OpenDropdown = NoDropdown; m_DropdownApply = {};
                    return true;
                }
            }
            m_OpenDropdown = NoDropdown; m_DropdownApply = {};
        }
        for (int i=0; i<m_Targets.size(); ++i) {
            if (m_Targets[i].rect.contains(point)) { activate(i); break; }
        }
    }
    // Suppress all remote input for the duration of the menu, including releases.
    switch (e.type) {
    case SDL_KEYUP: case SDL_MOUSEBUTTONUP: case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEMOTION: case SDL_MOUSEWHEEL: case SDL_CONTROLLERAXISMOTION:
    case SDL_CONTROLLERBUTTONUP: case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION:
        return true;
    default: return false;
    }
}
