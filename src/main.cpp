#include <fstream>
#include "App.h"

auto main() -> int {
    if (auto dat = std::ifstream("settings.dat"); dat) {
        auto& settings = Settings::settings;

        dat >> settings.lookUpCompiler >> settings.saveTabs;
        dat.ignore(1, '\n');
        std::getline(dat, settings.compilerPath);
        std::getline(dat, settings.flags);
        std::getline(dat, settings.includePath);
        std::getline(dat, settings.libPath);
        std::getline(dat, settings.compiler);
        if (settings.compiler.empty()) {
            settings.compiler = "g++.exe";
        }

        dat >> settings.width >> settings.height;

        if (settings.width < 800) {
            settings.width = 800;
        }
        if (settings.height < 600) {
            settings.height = 600;
        }

        dat >> settings.terminalHidden;
    }

    App().run();
}
