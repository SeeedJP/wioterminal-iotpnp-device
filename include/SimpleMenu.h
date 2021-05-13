#pragma once

#include <string>
#include <vector>
#include <LovyanGFX.hpp>
#include <AceButton.h>
using namespace ace_button;

class MenuItem
{
public:
    MenuItem(std::string label, uint32_t id) :
        label(label), id(id)
    {
    };

public:
    std::string label;
    uint32_t id;
};

class MenuItems
{
public:
    void Add(MenuItem item) {
        Items_.push_back(item);
    };

    std::size_t Size() {
        return Items_.size();
    };

    MenuItem Item(int i) {
        return Items_[i];
    };

private:
    std::vector<MenuItem> Items_;
};

class SimpleMenu
{
public:
    enum EventId
    {
        UP = 0,
        DOWN,
        PRESS,
    };

    SimpleMenu(LGFX& gfx);
    void init(MenuItems items);
    void init(MenuItems items, const char* title);
    MenuItem waitForSelection();
    void eventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState);

private:
    void drawTitle(const char* title);
    void drawItems();
    void updateCursor();
    void registerMenuItems(MenuItems items);

private:
    LGFX& Gfx_;

    MenuItems MenuItems_;
    uint16_t ItemHeight_;

    AceButton Buttons_[3];

    int8_t currentCursor;
    int8_t previousCursor;
    bool pressed;
};
