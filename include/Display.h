#pragma once

#include "SimpleMenu.h"

class LGFX;

class Display : public SimpleMenu
{
private:
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

public:
    Display(LGFX& gfx);
    void Init();
    void Clear();
    void SetBrightness(int brightness);
    void Printf(const char* format, ...);
    void PrintMessage(const char* message);

private:
    LGFX& Gfx_;

};
