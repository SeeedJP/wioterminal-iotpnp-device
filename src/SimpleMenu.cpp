#include <Arduino.h>
#include "SimpleMenu.h"

#define TITLE_HEIGHT    (60)
#define BOTTOM_PADDING  (20)

SimpleMenu* SimpleMenu::SimpleMenu_ = nullptr;

SimpleMenu::SimpleMenu(LGFX& gfx) :
    Gfx_(gfx)
{
    SimpleMenu_ = this;
}

void SimpleMenu::init(const MenuItems& items, const char* title)
{
    pinMode(WIO_5S_UP, INPUT_PULLUP);
    pinMode(WIO_5S_DOWN, INPUT_PULLUP);
    pinMode(WIO_5S_PRESS, INPUT_PULLUP);
    delay(100);

    Buttons_[static_cast<int>(EventId::UP)].init(WIO_5S_UP, HIGH, static_cast<uint8_t>(EventId::UP));
    Buttons_[static_cast<int>(EventId::DOWN)].init(WIO_5S_DOWN, HIGH, static_cast<uint8_t>(EventId::DOWN));
    Buttons_[static_cast<int>(EventId::PRESS)].init(WIO_5S_PRESS, HIGH, static_cast<uint8_t>(EventId::PRESS));

    ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
    buttonConfig->setEventHandler(buttonEventHandler);
    buttonConfig->setFeature(ButtonConfig::kFeatureClick);

    registerMenuItems(items);
    drawTitle(title);
    drawItems();

    currentCursor = 0;
    previousCursor = -1;
    pressed = false;
    updateCursor();
}

const MenuItem& SimpleMenu::waitForSelection()
{
    while (true)
    {
        for (int i = 0; i < static_cast<int>(sizeof(Buttons_) / sizeof(AceButton)); i++) Buttons_[i].check();
        if (pressed) break;
    }
    return MenuItems_.Item(currentCursor);
}

void SimpleMenu::eventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
    const uint8_t id = button->getId();

    switch (eventType)
    {
    case AceButton::kEventClicked:
        switch (static_cast<EventId>(id))
        {
        case EventId::UP:
            if (currentCursor > 0) 
            {
                currentCursor--;
                updateCursor();
            }
            break;
        case EventId::DOWN:
            if (currentCursor < static_cast<int8_t>(MenuItems_.Size()) - 1)
            {
                currentCursor++;
                updateCursor();
            }
            break;
        case EventId::PRESS:
            pressed = true;
            break;
        }
        break;
    }
}

void SimpleMenu::drawTitle(const char* title)
{
    Gfx_.setFont(&fonts::Font4);
    Gfx_.setTextDatum(textdatum_t::middle_center);
    Gfx_.drawString(title, Gfx_.width() / 2, TITLE_HEIGHT / 2);
}

void SimpleMenu::drawItems()
{
    Gfx_.setFont(&fonts::Font4);
    Gfx_.setTextDatum(textdatum_t::middle_center);
    Gfx_.setTextColor(TFT_WHITE, TFT_BLACK);

    for (size_t i = 0; i < MenuItems_.Size(); i++)
    {
        std::string label = MenuItems_.Item(i).label;
        Gfx_.drawString(label.c_str(), Gfx_.width() / 2, TITLE_HEIGHT + ItemHeight_ * i + ItemHeight_ / 2);
    }
}

void SimpleMenu::updateCursor()
{
    if (previousCursor != -1)
    {
        std::string label = MenuItems_.Item(previousCursor).label;
        Gfx_.setTextColor(TFT_WHITE, TFT_BLACK);
        Gfx_.drawString(label.c_str(), Gfx_.width() / 2, TITLE_HEIGHT + ItemHeight_ * previousCursor + ItemHeight_ / 2);
    }
    previousCursor = currentCursor;

    const std::string& label = MenuItems_.Item(currentCursor).label;
    Gfx_.setTextColor(TFT_BLACK, TFT_WHITE);
    Gfx_.drawString(label.c_str(), Gfx_.width() / 2, TITLE_HEIGHT + ItemHeight_ * currentCursor + ItemHeight_ / 2);
}

void SimpleMenu::registerMenuItems(const MenuItems& items)
{
    MenuItems_ = items;
    ItemHeight_ = (Gfx_.height() - TITLE_HEIGHT - BOTTOM_PADDING) / MenuItems_.Size();
}

void SimpleMenu::buttonEventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
    SimpleMenu_->eventHandler(button, eventType, buttonState);
}
