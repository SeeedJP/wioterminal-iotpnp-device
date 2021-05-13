#include <Arduino.h>
#include "Display.h"

#include <LovyanGFX.hpp>

static String StringVFormat(const char* format, va_list arg)
{
    const int len = vsnprintf(nullptr, 0, format, arg);
    char str[len + 1];
    vsnprintf(str, sizeof(str), format, arg);

    return String{ str };
}

Display::Display(LGFX& gfx) :
    SimpleMenu(gfx),
    Gfx_(gfx)
{
}

void Display::Init()
{
    Gfx_.begin();
    Gfx_.fillScreen(TFT_BLACK);
    Gfx_.setTextScroll(true);
	Gfx_.setBrightness(0);
}

void Display::Clear()
{
	Gfx_.clear();
    Gfx_.setCursor(0, 0);
}

void Display::SetBrightness(int brightness)
{
	Gfx_.setBrightness(brightness);
}

void Display::Printf(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    String str{ StringVFormat(format, arg) };
    va_end(arg);

    Gfx_.setTextColor(TFT_WHITE, TFT_BLACK);
    Gfx_.setFont(&fonts::Font2);
	Gfx_.print(str);
}

void Display::PrintMessage(const char* message)
{
    Gfx_.setTextColor(TFT_WHITE, TFT_BLACK);
    Gfx_.setTextFont(&fonts::Font4);
    Gfx_.setTextDatum(textdatum_t::middle_center);
    Gfx_.drawString(message, Gfx_.width() / 2, Gfx_.height() / 2);
}
