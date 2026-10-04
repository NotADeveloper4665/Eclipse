#include "streaming/controlcenter.h"
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QDebug>
#include <cmath>

static void check(bool value, const char* message)
{
    if (!value) qFatal("FAIL: %s", message);
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("EclipseTests");
    QCoreApplication::setApplicationName("ControlCenter");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    auto* prefs = StreamingPreferences::get();
    prefs->width = 1920; prefs->height = 1080; prefs->fps = 60; prefs->bitrateKbps = 20000;
    ControlCenter center(prefs);
    QVector<ControlCenter::Action> actions;
    center.action = [&](ControlCenter::Action action) { actions.append(action); };
    QSize size(1280,800);
    auto paint = [&](QString statistics = QString()) { return center.render(size,false,true,statistics); };
    auto click = [&](double x, double y) {
        const double scale = std::min({1.25,size.width()/1120.0,size.height()/690.0});
        SDL_Event e = {};
        e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
        e.button.x = std::lround((size.width()-1080*scale)/2 + x*scale);
        e.button.y = std::lround((size.height()-640*scale)/2 + y*scale);
        check(center.handleEvent(e,size),"mouse must be consumed");
        e.type = SDL_MOUSEBUTTONUP;
        check(center.handleEvent(e,size),"mouse release must be consumed");
        paint();
    };
    auto mouse = [&](Uint32 type, double x, double y) {
        const double scale = std::min({1.25,size.width()/1120.0,size.height()/690.0});
        const int px = std::lround((size.width()-1080*scale)/2 + x*scale);
        const int py = std::lround((size.height()-640*scale)/2 + y*scale);
        SDL_Event e = {}; e.type = type;
        if (type == SDL_MOUSEMOTION) {
            e.motion.x = px; e.motion.y = py;
        }
        else {
            e.button.button = SDL_BUTTON_LEFT; e.button.x = px; e.button.y = py;
        }
        check(center.handleEvent(e,size),"mouse drag event must be consumed");
    };
    auto pressKey = [&](SDL_Keycode keyCode) {
        SDL_Event e = {}; e.type = SDL_KEYDOWN; e.key.keysym.sym = keyCode;
        check(center.handleEvent(e,size),"keyboard selection must be consumed");
        paint();
    };
    auto image = paint();
    check(image.size()==size,"surface matches drawable");
    check(image.pixelColor(0,0).alpha()>0,"background scrim exists");
    click(285,39);
    check(actions.last()==ControlCenter::Fullscreen,"fullscreen action");
    click(60,180); // Video & Display
    click(610,195); // Open resolution dropdown
    click(610,301); // Select 2560 x 1440
    click(610,195); // Reopen the current value
    pressKey(SDLK_DOWN); pressKey(SDLK_RETURN); // Keyboard selects 3840 x 2160
    check(prefs->width==1920,"dropdown keyboard selection remains staged");
    click(610,195); pressKey(SDLK_UP); pressKey(SDLK_RETURN); // Restore 2560 x 1440
    check(center.dirty(),"edited draft is dirty");
    check(prefs->width==1920,"unsaved draft must not alter preferences");
    click(330,602); // Save
    check(!center.dirty() && prefs->width==2560 && prefs->height==1440,"resolution save");
    prefs->reload();
    check(prefs->width==2560,"settings persist through reload");
    click(70,335); // Network
    click(739,253); // drag the bitrate slider to its maximum
    click(330,602);
    check(prefs->bitrateKbps==150000,"bandwidth slider and persistence");
    mouse(SDL_MOUSEBUTTONDOWN,239,253);
    mouse(SDL_MOUSEMOTION,489,253);
    mouse(SDL_MOUSEBUTTONUP,489,253);
    mouse(SDL_MOUSEMOTION,739,253);
    click(330,602);
    check(prefs->bitrateKbps>=75000 && prefs->bitrateKbps<=76000,"release stops bitrate slider dragging");
    click(45,128); // General
    click(350,395);
    check(actions.last()!=ControlCenter::Disconnect,"disconnect needs confirmation");
    click(350,395);
    check(actions.last()==ControlCenter::Disconnect,"confirmed disconnect");
    SDL_Event key = {}; key.type = SDL_KEYDOWN; key.key.keysym.sym = SDLK_ESCAPE;
    check(center.handleEvent(key,size) && actions.last()==ControlCenter::Close,"Escape closes locally");
    SDL_Event controller = {}; controller.type = SDL_CONTROLLERBUTTONDOWN;
    controller.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    check(center.handleEvent(controller,size) && actions.last()==ControlCenter::Close,"controller B closes locally");
    SDL_Event motion = {}; motion.type = SDL_MOUSEMOTION;
    check(center.handleEvent(motion,size),"mouse motion cannot reach host");
    SDL_Event frame = {}; frame.type = SDL_USEREVENT;
    check(!center.handleEvent(frame,size),"stream rendering events remain dispatchable");
    for (QSize testSize : {QSize(480,320),QSize(800,600),QSize(1920,1080),QSize(3840,2160)}) {
        size=testSize; check(paint().size()==size,"resized surface");
        click(70,180); click(610,195); click(610,301);
        check(center.dirty(),"scaled mouse targets remain usable");
    }
    size={1280,800}; paint(); click(45,128);
    paint().save("eclipse-preview.png");
    for (int page=0; page<6; ++page) {
        click(65,128+52*page);
        paint().save(QString("eclipse-page-%1.png").arg(page));
    }
    size={1280,800};
    click(65,180); // Video & Display
    click(610,195); // Capture the open dropdown for the UI preview
    paint().save("eclipse-settings-dropdown-preview.png");
    prefs->width = 1920; prefs->height = 1080; prefs->fps = 60;
    prefs->bitrateKbps = 20000; prefs->audioConfig = StreamingPreferences::AC_STEREO;
    prefs->save();
    StreamingPreferences::saveHostStreamProfile("host-a");
    StreamingPreferences::saveHostStreamProfile("host-b");
    auto* host = StreamingPreferences::createForHost("host-a");
    check(host != nullptr, "host profile loads");
    host->width = 2560; host->height = 1440; host->fps = 120;
    host->bitrateKbps = 45000; host->audioConfig = StreamingPreferences::AC_51_SURROUND;
    host->videoCodecConfig = StreamingPreferences::VCC_FORCE_HEVC;
    host->save();
    delete host;
    prefs->reload();
    check(prefs->width == 1920 && prefs->height == 1080 && prefs->fps == 60 &&
          prefs->bitrateKbps == 20000 && prefs->audioConfig == StreamingPreferences::AC_STEREO,
          "host save must preserve global stream defaults");
    check(prefs->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC,
          "shared codec preference persists");
    host = StreamingPreferences::createForHost("host-a");
    check(host->width == 2560 && host->height == 1440 && host->fps == 120 &&
          host->bitrateKbps == 45000 && host->audioConfig == StreamingPreferences::AC_51_SURROUND,
          "reconnect reloads saved host overrides");
    delete host;
    host = StreamingPreferences::createForHost("host-b");
    check(host->width == 1920 && host->fps == 60 && host->bitrateKbps == 20000,
          "saving one host preserves other host profiles");
    delete host;
    StreamingPreferences::clearHostStreamProfile("host-a");
    check(StreamingPreferences::createForHost("host-a") == nullptr, "cleared profile uses defaults");
    prefs->fsrMode = 3;
    prefs->save();
    prefs->fsrMode = 0;
    prefs->reload();
    check(prefs->fsrMode == 3, "FSR preset persists globally");
    StreamingPreferences::saveHostStreamProfile("fsr-host");
    prefs->fsrMode = 0;
    prefs->save();
    host = StreamingPreferences::createForHost("fsr-host");
    check(host && host->fsrMode == 3 && prefs->fsrMode == 0, "FSR preset remains isolated per host");
    delete host;
    qInfo("PASS: control-center interactions, persistence, input suppression, scaling, and host profile isolation");
    return 0;
}
