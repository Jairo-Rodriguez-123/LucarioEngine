/**
 * @file Main.cpp
 * @brief Punto de entrada Win32 de Wildvine Engine.
 */
#include "BaseApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    BaseApp app;
    return app.run(hInstance, nCmdShow);
}
