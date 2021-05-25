#pragma once

#include <string>
#include <vector>
#include <LovyanGFX.hpp>
#include <AceButton.h>
using namespace ace_button;

struct MenuItem
{
    std::string label;
    uint32_t id;
};

class MenuItems
{
public:
    void Add(const MenuItem& item)
    {
        Items_.push_back(item);
    }

    size_t Size() const
    {
        return Items_.size();
    }

    const MenuItem& Item(int i)
    {
        return Items_[i];
    }

private:
    std::vector<MenuItem> Items_;

};

class SimpleMenu
{
public:
    enum class EventId
    {
        UP = 0,
        DOWN,
        PRESS,
    };

    SimpleMenu(LGFX& gfx);
    void init(const MenuItems& items, const char* title);
    const MenuItem& waitForSelection();
    
    void eventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState);

private:
    void drawTitle(const char* title);
    void drawItems();
    void updateCursor();
    void registerMenuItems(const MenuItems& items);

private:
    LGFX& Gfx_;

    MenuItems MenuItems_;
    uint16_t ItemHeight_;

    AceButton Buttons_[3];

    int8_t currentCursor;
    int8_t previousCursor;
    bool pressed;

    static SimpleMenu* SimpleMenu_;

    static void buttonEventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState);

};
