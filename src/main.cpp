#include <fstream>
#include "App.h"

auto main() -> int {
    if (auto dat = std::ifstream("settings.dat"); dat) {
        dat >> Settings::settings.lookUpCompiler >> Settings::settings.saveTabs;
        dat.ignore(1, '\n');
        std::getline(dat, Settings::settings.compilerPath);
        std::getline(dat, Settings::settings.flags);
        std::getline(dat, Settings::settings.includePath);
        std::getline(dat, Settings::settings.libPath);
        std::getline(dat, Settings::settings.compiler);
        if (Settings::settings.compiler.empty()) {
            Settings::settings.compiler = "g++";
        }

        dat >> Settings::settings.width >> Settings::settings.height;

        if (Settings::settings.width < 800) {
            Settings::settings.width = 800;
        }
        if (Settings::settings.height < 600) {
            Settings::settings.height = 600;
        }

        dat >> Settings::settings.terminalHidden;
    }

    App().run();
}
