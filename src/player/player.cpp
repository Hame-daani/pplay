//
// Created by cpasjuste on 03/10/18.
//

#include <sstream>
#include "main.h"
#include "player.h"
#include "player_osd.h"
#include "video_texture.h"
#include "utility.h"

using namespace c2d;

Player::Player(Main *_main) : Rectangle(_main->getSize()) {

    main = _main;

    Vector2f pos = main->getSize();
    Player::setPosition(pos);
    Player::setOrigin(Origin::BottomRight);

    tweenScale = new TweenScale({0.25f, 0.25f}, {1.0f, 1.0f}, 0.5f);
    Player::add(tweenScale);
    tweenPosition = new TweenPosition({pos.x - 16, pos.y - 16}, {pos}, 0.5f);
    Player::add(tweenPosition);

    Player::setVisibility(Visibility::Hidden);

    mpv = new Mpv(main->getIo()->getDataPath() + "mpv", true);

#ifndef FULL_TEXTURE_TEST
    texture = new VideoTexture(main, pos);
    texture->setOutlineColor(Color::Red);
    texture->setOutlineThickness(4);
#else
    texture = new VideoTexture(main, {64, 64});
    texture->setOrigin(Origin::Center);
    texture->setPosition(Vector2f(Player::getSize().x / 2, Player::getSize().y / 2));
#endif
    Player::add(texture);

    osd = new PlayerOSD(main);
    Player::add(osd);
}

Player::~Player() {
    delete (mpv);
}

bool Player::load(const MediaFile &f) {

    file = f;
    std::string path = file.path;
#ifdef __SMB2__
#if 0
    if (Utility::startWith(path, "smb://")) {
        std::replace(path.begin(), path.end(), '\\', '/');
    }
#endif
#endif

    // disable subtitles if slang option not set in "mpv.conf" (overridden by "watch_later")
    char *slang = mpv_get_property_string(mpv->getHandle(), "slang");
    if (strlen(slang) <= 0) {
        printf("slang not set, disabling subtitles by default");
        mpv_set_option_string(mpv->getHandle(), "sid", "no");
    }

    int res = mpv->load(path, Mpv::LoadType::Replace, "pause=yes,speed=1");
    if (res != 0) {
        main->getStatus()->show("Error...", "Could not play file:\n" + std::string(mpv_error_string(res)));
        printf("Player::load: could not play file: %s\n", mpv_error_string(res));
        return false;
    }

    return true;
}

void Player::onLoadEvent() {
    // load/update media information
    file.mediaInfo = mpv->getMediaInfo(file);
    file.mediaInfo.save(file);

    // TODO: is this really needed as it should be extracted from scrapper
    // main->getFiler()->setMediaInfo(file, file.mediaInfo);

    // build video track selection menu
    if (!file.mediaInfo.videos.empty()) {
        // videos menu options
        std::vector<MenuItem> items;
        for (auto &stream: file.mediaInfo.videos) {
            items.emplace_back(stream.title + "\n" + stream.language + " " + stream.codec + " "
                               + std::to_string(stream.width) + "x" + std::to_string(stream.height),
                               "", MenuItem::Position::Top, stream.id);
        }
        menuVideoStreams = new MenuVideoSubmenu(
                main, main->getMenuVideo()->getGlobalBounds(), "VIDEO", items, MENU_VIDEO_TYPE_VID);
        menuVideoStreams->setVisibility(Visibility::Hidden, false);
        menuVideoStreams->setLayer(3);
        add(menuVideoStreams);
    }

    // build audio track selection menu
    if (!file.mediaInfo.audios.empty()) {
        // audios menu options
        std::vector<MenuItem> items;
        for (auto &stream: file.mediaInfo.audios) {
            items.emplace_back(stream.title + "\n" + stream.language + " "
                               + stream.codec + " " + std::to_string(stream.channels) + "ch "
                               + std::to_string(stream.sample_rate / 1000) + " Khz",
                               "", MenuItem::Position::Top, stream.id);
        }
        menuAudioStreams = new MenuVideoSubmenu(
                main, main->getMenuVideo()->getGlobalBounds(), "AUDIO", items, MENU_VIDEO_TYPE_AUD);
        menuAudioStreams->setVisibility(Visibility::Hidden, false);
        menuAudioStreams->setLayer(3);
        add(menuAudioStreams);
    }

    // build subtitles track selection menu
    if (!file.mediaInfo.subtitles.empty()) {
        // subtitles menu options
        std::vector<MenuItem> items;
        items.emplace_back("None", "", MenuItem::Position::Top, -1);
        for (auto &stream: file.mediaInfo.subtitles) {
            items.emplace_back(stream.title + "\nLang: " + stream.language, "", MenuItem::Position::Top, stream.id);
        }
        menuSubtitlesStreams = new MenuVideoSubmenu(
                main, main->getMenuVideo()->getGlobalBounds(), "SUBTITLES", items, MENU_VIDEO_TYPE_SUB);
        menuSubtitlesStreams->setVisibility(Visibility::Hidden, false);
        menuSubtitlesStreams->setLayer(3);
        add(menuSubtitlesStreams);
    }

#ifdef FULL_TEXTURE_TEST
    texture->resize({file.mediaInfo.videos.at(0).width,
                     file.mediaInfo.videos.at(0).height});
    float scaling = std::min(
            getSize().x / texture->getSize().x,
            getSize().y / texture->getSize().y);
    texture->setScale(scaling, scaling);
#endif

    resume();
    setFullscreen(true);
}

void Player::onStopEvent(int reason) {
    main->getStatus()->hide();
    main->getMenuVideo()->reset();
    osd->reset();

    if (reason == MPV_END_FILE_REASON_ERROR) {
        main->getStatus()->show("Error...", "Could not load file");
        printf("Player::load: could not load file\n");
    }

    // audio
    if (menuAudioStreams != nullptr) {
        delete (menuAudioStreams);
        menuAudioStreams = nullptr;
    }
    // video
    if (menuVideoStreams != nullptr) {
        delete (menuVideoStreams);
        menuVideoStreams = nullptr;
    }
    // subtitles
    if (menuSubtitlesStreams != nullptr) {
        delete (menuSubtitlesStreams);
        menuSubtitlesStreams = nullptr;
    }

    pplay::Utility::setCpuClock(pplay::Utility::CpuClock::Min);
#ifdef __SWITCH__
    appletSetMediaPlaybackState(false);
#endif

    if (main->isExiting()) {
        main->setRunningStop();
    } else if (mpv->isStopped()) {
        setFullscreen(false, true);
    }
}

void Player::onUpdate() {
    //TODO: cache-buffering-state
    if (mpv->isAvailable()) {
        mpv_event *event = mpv->getEvent();
        if (event != nullptr) {
            switch (event->event_id) {
                case MPV_EVENT_START_FILE:
                    printf("MPV_EVENT_START_FILE\n");
                    main->getStatus()->show("Please Wait...", "Loading... " + file.name, true);
                    break;
                case MPV_EVENT_FILE_LOADED:
                    printf("MPV_EVENT_FILE_LOADED\n");
                    onLoadEvent();
                    main->getStatus()->hide();
                    break;
                case MPV_EVENT_END_FILE:
                    printf("MPV_EVENT_END_FILE\n");
                    onStopEvent(((mpv_event_end_file *) event->data)->reason);
                    break;
                default:
                    break;
            }
        }
    }

    if (isVisible()) {
        Rectangle::onUpdate();
    }
}

bool Player::onInput(c2d::Input::Player *players) {
    unsigned int keys = players[0].keys;

    if (mpv->isStopped()
        || main->getFiler()->isVisible()
        || main->getMenuVideo()->isVisible()
        || (getMenuVideoStreams() != nullptr && getMenuVideoStreams()->isVisible())
        || (getMenuAudioStreams() != nullptr && getMenuAudioStreams()->isVisible())
        || (getMenuSubtitlesStreams() != nullptr && getMenuSubtitlesStreams()->isVisible())) {
        return C2DObject::onInput(players);
    }

    if (keys & c2d::Input::Key::Fire5) {
        setSpeed(1);
    } else if (keys & c2d::Input::Key::Fire6) {
        double new_speed = mpv->getSpeed() * 2;
        if (new_speed <= 100) {
            setSpeed(new_speed);
        }
    }
#ifdef __PS4__
    else if (keys & c2d::Input::Key::Select) {
        //osd->setVisibility(c2d::Visibility::Visible);
        getMpv()->seek(-5.0);
    } else if (keys & c2d::Input::Key::Start) {
        //osd->setVisibility(c2d::Visibility::Visible);
        getMpv()->seek(5.0);
    }
#endif

    else if (keys & c2d::Input::Key::Fire4) {
	main->getPlayer()->resume();
	main->getStatus()->show("Info...", "Resuming playback...");
    } else if (keys & c2d::Input::Key::Fire3) {
	main->getPlayer()->pause();
	main->getStatus()->show("Info...", "Pausing playback...");
    }

    if (osd->isVisible()) {
        return C2DObject::onInput(players);
    }

    //////////////////
    /// handle inputs
    //////////////////
    if ((keys & Input::Key::Fire1) || (keys & Input::Key::Up)) {
        if (!osd->isVisible()) {
            osd->setVisibility(Visibility::Visible, true);
            main->getStatusBar()->setVisibility(Visibility::Visible, true);
        }
    } else if (keys & c2d::Input::Key::Right || keys & Input::Key::Fire2) {
        setFullscreen(false);
    } else if (keys & c2d::Input::Key::Left) {
        main->getMenuVideo()->setVisibility(Visibility::Visible, true);
    }

    return true;
}

void Player::setVideoStream(int streamId) {
    printf("Player::setVideoStream: %i\n", streamId);
    mpv->setVid(streamId);
}

void Player::setAudioStream(int streamId) {
    printf("Player::setAudioStream: %i\n", streamId);
    mpv->setAid(streamId);
}

void Player::setSubtitleStream(int streamId) {
    printf("Player::setSubtitleStream: %i\n", streamId);
    mpv->setSid(streamId);
}

int Player::getVideoStream() {
    return mpv->getVid();
}

int Player::getAudioStream() {
    return mpv->getAid();
}

int Player::getSubtitleStream() {
    return mpv->getSid();
}

void Player::setSpeed(double speed) {
    mpv->setSpeed(speed);
    osd->setVisibility(Visibility::Visible, true);
}

void Player::pause() {
    mpv->pause();
    pplay::Utility::setCpuClock(pplay::Utility::CpuClock::Min);
#ifdef __SWITCH__
    appletSetMediaPlaybackState(false);
#endif
}

void Player::resume() {
    mpv->resume();
#ifdef __SWITCH__
    if (main->getConfig()->getOption(OPT_CPU_BOOST)->getString() == "Enabled") {
        pplay::Utility::setCpuClock(pplay::Utility::CpuClock::Max);
    }
    appletSetMediaPlaybackState(true);
#endif
}

void Player::stop() {
    mpv->stop();
}

bool Player::isFullscreen() {
    return fullscreen;
}

void Player::setFullscreen(bool fs, bool hide) {

    if (fs == fullscreen) {
        if (hide) {
            setVisibility(Visibility::Hidden, true);
        }
        return;
    }

    fullscreen = fs;

    if (!fullscreen) {
        if (hide) {
            setVisibility(Visibility::Hidden, true);
        } else {
            tweenScale->play(TweenDirection::Backward);
            tweenPosition->play(TweenDirection::Backward);
        }
        texture->showFade();
        main->getMenuVideo()->setVisibility(Visibility::Hidden, true);
        if (menuVideoStreams != nullptr) {
            menuVideoStreams->setVisibility(Visibility::Hidden, true);
        }
        if (menuAudioStreams != nullptr) {
            menuAudioStreams->setVisibility(Visibility::Hidden, true);
        }
        if (menuSubtitlesStreams != nullptr) {
            menuSubtitlesStreams->setVisibility(Visibility::Hidden, true);
        }
        main->getFiler()->setVisibility(Visibility::Visible, true);
        main->getStatusBar()->setVisibility(Visibility::Visible, true);
    } else {
        texture->hideFade();
        main->getFiler()->setVisibility(Visibility::Hidden, true);
        main->getStatusBar()->setVisibility(Visibility::Hidden, true);
        setVisibility(Visibility::Visible, true);
    }
}

MenuVideoSubmenu *Player::getMenuVideoStreams() {
    return menuVideoStreams;
}

MenuVideoSubmenu *Player::getMenuAudioStreams() {
    return menuAudioStreams;
}

MenuVideoSubmenu *Player::getMenuSubtitlesStreams() {
    return menuSubtitlesStreams;
}

const std::string &Player::getTitle() const {
    return file.name;
}

PlayerOSD *Player::getOSD() {
    return osd;
}

Mpv *Player::getMpv() {
    return mpv;
}
