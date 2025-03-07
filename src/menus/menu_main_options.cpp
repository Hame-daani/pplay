//
// Created by cpasjuste on 07/12/18.
//

#include "main.h"
#include "menu_main_options.h"

using namespace c2d;

MenuMainOptions::MenuMainOptions(Main *main, const c2d::FloatRect &rect, const std::vector<MenuItem> &items)
        : Menu(main, rect, "OPTIONS", items, true) {
}

void MenuMainOptions::onOptionSelection(MenuItem *item) {
    setVisibility(Visibility::Hidden);
    main->getMenuMain()->getMenuMainOptionsSubmenu(item->name)->setVisibility(Visibility::Visible);
}

bool MenuMainOptions::onInput(c2d::Input::Player *players) {
    if (players[0].keys & Input::Left || players[0].keys & Input::Key::Fire2) {
        setVisibility(Visibility::Hidden, true);
        main->getMenuMain()->setVisibility(Visibility::Visible, true);
        return true;
    }

    if (players[0].keys & Input::Right) {
        onOptionSelection(getSelection());
        return true;
    }

    return Menu::onInput(players);
}
