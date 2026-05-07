// Android globals — defines the extern variables referenced from the BeebEm core
// (originally declared in Main.h / macos/MacMain.h).

#include "Windows.h"
#include "BeebWin.h"
#include "Model.h"

// argc/argv passed to BeebWin::ParseCommandLine().
int    __argc = 0;
char **__argv = nullptr;

int       done               = 0;
Model     MachineType        = Model::B;
BeebWin  *mainWin            = nullptr;
HINSTANCE hInst              = nullptr;
HWND      hCurrentDialog     = nullptr;
HACCEL    hCurrentAccelTable = nullptr;

void Quit()            { done = 1; }
bool ToggleFullScreen() { return false; }
void ShowingMenu()     {}
void NoMenuShown()     {}
