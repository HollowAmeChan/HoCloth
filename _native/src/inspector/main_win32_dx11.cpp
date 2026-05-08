#include "hocloth/inspector/inspector_app.hpp"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    return hocloth::inspector::RunInspectorApp();
}
