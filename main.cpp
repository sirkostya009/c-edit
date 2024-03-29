#include <AppCore/AppCore.h>
#include <iostream>
#include <boost/process.hpp>
#include <boost/process/windows.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <string>
#include <thread>
#include <queue>
#include <utility>
#include <shobjidl.h>

namespace ul = ultralight;
namespace bp = boost::process;
namespace bf = boost::filesystem;
namespace ba = boost::asio;

#define OnSettingsChange(name) \
    ul::JSCallback([] (const ul::JSObject &thisObject, const ul::JSArgs &args) { \
        if (!args.empty() && args[0].IsString()) { \
            settings.name = ((ul::String) args[0]).utf8().data(); \
            std::cout << settings.name << std::endl; \
        } \
    })

#define OnSettingsChangeBoolean(name) \
    ul::JSCallback([] (const ul::JSObject &thisObject, const ul::JSArgs &args) { \
        if (!args.empty() && args[0].IsBoolean()) { \
            settings.name = args[0].ToBoolean(); \
            std::cout << settings.name << std::endl; \
        } \
    })

class Settings : public ul::WindowListener, public ul::LoadListener, public ul::ViewListener {
    ul::RefPtr<ul::Window> window;
    ul::RefPtr<ul::Overlay> overlay;
    std::function<void()> closeCallback;

public:
    struct settings_t {
        bool lookUpCompiler;
        bool saveTabs;
        std::string compilerPath;
        std::string flags;
        std::string includePath;
        std::string libPath;
        std::string compiler;

        // internal
        uint32_t width, height;
        bool terminalHidden;
    };

    static settings_t settings;
    static uint32_t settingsHeight;
    static uint32_t settingsWidth;

    Settings(const ul::RefPtr<ul::App>& app, std::function<void()> onClose)
    : closeCallback{std::move(onClose)}
    , window{ ul::Window::Create(app->main_monitor(), settingsWidth, settingsHeight, false, ul::kWindowFlags_Resizable) }
    , overlay{ ul::Overlay::Create(window, settingsWidth, settingsHeight, 0, 0) }
    {
        window->set_listener(this);
        overlay->view()->set_load_listener(this);
        overlay->view()->set_view_listener(this);

        window->MoveToCenter();
        overlay->view()->LoadURL("file:///settings.html");
        overlay->Focus();
    }

    void OnAddConsoleMessage(ultralight::View *caller, ultralight::MessageSource source, ultralight::MessageLevel level,
                             const ultralight::String &message, uint32_t line_number, uint32_t column_number,
                             const ultralight::String &source_id) override {
        std::cout << "Console: " << message.utf8().data() << " at line: " << line_number << ", column: " << column_number << std::endl;
    }

    void OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) override {
        ul::SetJSContext(caller->LockJSContext()->ctx());
        auto global = ul::JSGlobalObject();

        global["OnBinChange"]      = OnSettingsChange(compilerPath);
        global["OnLibChange"]      = OnSettingsChange(libPath);
        global["OnFlagsChange"]    = OnSettingsChange(flags);
        global["OnIncludeChange"]  = OnSettingsChange(includePath);
        global["OnCompilerChange"] = OnSettingsChange(compiler);
        global["OnLookUpChange"]   = OnSettingsChangeBoolean(lookUpCompiler);
        global["OnSaveTabsChange"] = OnSettingsChangeBoolean(saveTabs);

        ul::JSEval(("lookUpCompiler.checked = " + std::to_string(settings.lookUpCompiler)).c_str());
        ul::JSEval(("bin.value = '" + settings.compilerPath + '\'').c_str());
        ul::JSEval(("flags.value = '" + settings.flags + '\'').c_str());
        ul::JSEval(("include.value = '" + settings.includePath + '\'').c_str());
        ul::JSEval(("lib.value = '" + settings.libPath + '\'').c_str());
        ul::JSEval(("compiler.value = '" + settings.compiler + '\'').c_str());
    }

    void OnClose(ul::Window*) override {
        settingsWidth = window->width();
        settingsHeight = window->height();
        window->Close();
        closeCallback();
    }

    void OnResize(ul::Window*, uint32_t width, uint32_t height) override {
        overlay->Resize(width, height);
    }

    void OnChangeTitle(ul::View *caller, const ul::String &title) override {
        window->SetTitle(title.utf8().data());
    }

    bool OnKeyEvent(const ultralight::KeyEvent &evt) override {
        constexpr auto Escape = 27;

        switch (evt.virtual_key_code) {
        case Escape:
            OnClose(window.get());
        }
        return true;
    }
};

Settings::settings_t Settings::settings{
    .lookUpCompiler = true,
    .compiler = "g++",
    .width = 800,
    .height = 600
};

uint32_t Settings::settingsHeight = 300;
uint32_t Settings::settingsWidth = 400;

class App : public ul::AppListener, public ul::WindowListener, public ul::LoadListener, public ul::ViewListener {
    ul::RefPtr<ul::App> app;
    ul::RefPtr<ul::Window> window;
    ul::RefPtr<ul::Overlay> overlay;

    bool isRunning = false;
    bp::async_pipe* in = nullptr;

    struct callback {
        std::string msg;

        explicit callback(std::string &&msg) : msg{std::move(msg)} {}
    };

    std::queue<callback> queue;

    Settings* settings = nullptr;
    bool closeSettings = false;
public:
    App()
    : app{ ul::App::Create() }
    , window{ ul::Window::Create(app->main_monitor(),
                                 Settings::settings.width,
                                 Settings::settings.height,
                                 false,
                                 ul::kWindowFlags_Resizable | ul::kWindowFlags_Maximizable) }
    , overlay{ ul::Overlay::Create(window, 1, 1, 0, 0) }
    {
        app->set_listener(this);
        window->set_listener(this);
        overlay->view()->set_load_listener(this);
        overlay->view()->set_view_listener(this);

        window->MoveToCenter();
        overlay->Resize(window->width(), window->height());
        overlay->view()->LoadURL("file:///app.html");
        overlay->Focus();
    }

    inline void run() {
        app->Run();
    }

    inline ul::String getContents() {
        return (ul::String) ul::JSEval("codearea.value");
    }

    inline void print(const std::string& str) {
        queue.emplace("terminal.value += '" + str + '\'');
    }

    inline void status(const std::string& str) {
        queue.emplace("status.innerText = '" + str + '\'');
    }

    inline void toggleTerminal() {
        queue.emplace("terminal.toggleAttribute('readonly')");
    }

    void OnUpdate() override {
        while (!queue.empty()) {
            ul::JSEval(queue.front().msg.c_str());
            queue.pop();
        }

        if (closeSettings) {
            delete settings;
            settings = nullptr;
            closeSettings = false;
            ul::SetJSContext(overlay->view()->LockJSContext()->ctx());
        }
    }

    void OnClose(ul::Window*) override {
        std::ofstream("./settings.dat") << Settings::settings.lookUpCompiler << Settings::settings.saveTabs << '\n'
                                        << Settings::settings.compilerPath << '\n'
                                        << Settings::settings.flags << '\n'
                                        << Settings::settings.includePath << '\n'
                                        << Settings::settings.libPath << '\n'
                                        << Settings::settings.compiler << '\n'
                                        << window->width() << ' ' << window->height() << '\n'
                                        << ul::JSEval("terminalSection.classList.contains('hidden')").ToBoolean();

        std::exit(0);
    }

    void OnResize(ul::Window*, uint32_t width, uint32_t height) override {
        overlay->Resize(width, height);
    }

    void OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) override {
        using ul::JSCallback, ul::JSCallbackWithRetval;
        ul::SetJSContext(caller->LockJSContext()->ctx());
        auto global = ul::JSGlobalObject();

        global["OnRun"]          = BindJSCallback(&App::OnRun);
        global["OnStdIn"]        = BindJSCallback(&App::OnStdIn);
        global["SaveFile"]       = BindJSCallback(&App::SaveFile);
        global["OpenSettings"]   = BindJSCallback(&App::OpenSettings);
        global["OpenFileDialog"] = BindJSCallbackWithRetval(&App::OpenFileDialog);
        global["SaveFileDialog"] = BindJSCallbackWithRetval(&App::SaveFileDialog);

        if (Settings::settings.terminalHidden) {
            ul::JSEval("toggleTerminal()");
        }
    }

    void OnChangeCursor(ul::View *caller, ul::Cursor cursor) override {
        window->SetCursor(cursor);
    }

    void OnChangeTitle(ul::View *caller, const ul::String &title) override {
        window->SetTitle(title.utf8().data());
    }

    void OnAddConsoleMessage(ul::View*, ul::MessageSource, ul::MessageLevel, const ul::String &message,
                             uint32_t line_number, uint32_t column_number, const ul::String &) override {
        std::cout << "Console: " << message.utf8().data() << " at line: " << line_number << ", column: " << column_number << std::endl;
    }

    void OnRun(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        if (isRunning) return;
        isRunning = true;

        ul::JSEval("terminal.value = ''");
        toggleTerminal();
        std::thread([&, filename = std::move(args[0].IsString()
                                      ? std::string(((ul::String) args[0]).utf8().data())
                                      : ((bf::temp_directory_path() / bf::unique_path()).string() + ".cpp"))] {
            std::ofstream(filename) << getContents().utf8().data();

            auto out = bp::ipstream();
            auto err = std::error_code();
            auto exe = bf::temp_directory_path() / bf::unique_path();
            auto compiler = Settings::settings.lookUpCompiler
                    ? bp::search_path(Settings::settings.compiler)
                    : bf::path(Settings::settings.compilerPath) / Settings::settings.compiler;
            status("Compiling");
            auto compilation = bp::child(compiler, filename, "-o", exe,
//                                         Settings::settings.flags,
//                                         "-I", Settings::settings.includePath,
//                                         "-L", Settings::settings.libPath,
                                         (bp::std_err & bp::std_out) > out,
                                         err, bp::windows::create_no_window);
            auto read = std::string();
            while (std::getline(out, read)) {
                if (!read.empty()) print(read);
            }

            compilation.wait();
            if (err || compilation.exit_code()) {
                toggleTerminal();
                print("Compilation failed\\n" + err.message());
                status("Failed");
                isRunning = false;
                return;
            }

            auto ioc = ba::io_context();
            in = new bp::async_pipe(ioc);
            out = bp::ipstream{};
            status("Running");
            toggleTerminal();
            auto child = bp::child(exe.string() + ".exe",
                                   (bp::std_err & bp::std_out) > out,
                                   bp::std_in < *in,
                                   ioc, err, bp::windows::create_no_window);
            if (err) {
                print("Execution failed\\n" + err.message());
                status("Failed");
                isRunning = false;
                return;
            }

            while (std::getline(out, read)) {
                if (!read.empty()) print(read);
            }

            child.wait();
            isRunning = false;
            print("\\nProcess finished with exit code " + std::to_string(child.exit_code()));
            status("Finished");
            delete in;
            in = nullptr;
        }).detach();
    }

    ul::JSValue OpenFileDialog(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        IFileOpenDialog *dialog;
        auto hr = CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_ALL,
                IID_IFileOpenDialog,
                reinterpret_cast<LPVOID*>(&dialog)
        );
        if (FAILED(hr)) {
            return {};
        }

        auto handle = (HWND) window->native_handle();

        hr = dialog->Show(handle);
        if (FAILED(hr)) {
            return {};
        }

        IShellItem *item;
        hr = dialog->GetResult(&item);
        if (FAILED(hr)) {
            return {};
        }

        LPWSTR filePath;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
        if (FAILED(hr)) {
            return {};
        }

        CoTaskMemFree(filePath);
        item->Release();
        dialog->Release();

        auto file = std::ifstream(filePath);
        auto str = std::string(std::istreambuf_iterator<char>(file), {});

        auto array = ul::JSArray{};
        array.push(str.c_str());
        auto wstr = std::wstring(filePath);
        array.push(std::string(wstr.begin(), wstr.end()).c_str());
        return {array};
    }

    ul::JSValue SaveFileDialog(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        if (args.empty() || !args[0].IsString()) {
            std::cout << "SaveFileDialog invalid args";
            return {};
        }

        auto defaultExtension = (ul::String) args[0];

        IFileSaveDialog* dialog;
        auto hr = CoCreateInstance(
                CLSID_FileSaveDialog,
                nullptr,
                CLSCTX_ALL,
                IID_IFileSaveDialog,
                reinterpret_cast<LPVOID*>(&dialog)
        );
        if (FAILED(hr)) {
            return {};
        }

        auto handle = (HWND) window->native_handle();

        dialog->SetDefaultExtension(reinterpret_cast<LPCWSTR>(defaultExtension.utf8().data()));
        COMDLG_FILTERSPEC filterSpec[] = {
                {L"All Files", L"*.*"},
                {L"C++ Files", L"*.cpp"},
                {L"C Files", L"*.c"},
        };
        dialog->SetFileTypes(3, filterSpec);

        hr = dialog->Show(handle);
        if (FAILED(hr)) {
            dialog->Release();
            return {};
        }

        IShellItem* item;
        hr = dialog->GetResult(&item);
        if (FAILED(hr)) {
            dialog->Release();
            return {};
        }

        LPWSTR filePath;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
        if (FAILED(hr)) {
            item->Release();
            dialog->Release();
            return {};
        }

        std::ofstream(filePath) << getContents().utf8().data();

        std::wstring result(filePath);
        CoTaskMemFree(filePath);
        item->Release();
        dialog->Release();

        return {std::string(result.begin(), result.end()).c_str()};
    }

    void SaveFile(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        if (args.size() < 2 || !args[0].IsString() || !args[1].IsString()) {
            std::cout << "SaveFile invalid args";
            return;
        }

        std::ofstream(((ul::String) args[0]).utf8().data()) << ((ul::String) args[1]).utf8().data();
    }

    bool compareString(const ul::String8 &a, const char* str) {
        return !strncmp(a.data(), str, a.length());
    }

    void OnStdIn(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        if (in == nullptr) {
            return;
        }

        if (args.empty() || !args[0].IsString()) {
            std::cout << "OnStdIn invalid args" << std::endl;
            return;
        }

        auto string = ((ul::String) args[0]).utf8();
        char key;

        if (string.length() > 1) {
            if (compareString(string, "Enter")) {
                key = '\n';
            } else if (compareString(string, "Tab")) {
                key = '\t';
            } else if (compareString(string, "Backspace")) {
                key = '\b';
            } else if (compareString(string, "Delete")) {
                key = 127;
            } else {
                return;
            }
        } else {
            key = string[0];
        }

        ba::async_write(*in, ba::buffer(&key, 1), [](const std::error_code &err, std::size_t) {
            if (err) std::cout << "OnStdIn error" << std::endl;
        });
    }

    void OpenSettings(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        if (settings) {
            return;
        }

        settings = new Settings(app, [this] { closeSettings = true; });
    }
};

auto main() -> int {
    CoInitialize(nullptr);

    if (auto datFile = std::ifstream("./settings.dat"); datFile.is_open()) {
        datFile >> Settings::settings.lookUpCompiler >> Settings::settings.saveTabs;
        datFile.ignore(1, '\n');
        std::getline(datFile, Settings::settings.compilerPath);
        std::getline(datFile, Settings::settings.flags);
        std::getline(datFile, Settings::settings.includePath);
        std::getline(datFile, Settings::settings.libPath);
        std::getline(datFile, Settings::settings.compiler);
        if (Settings::settings.compiler.empty()) {
            Settings::settings.compiler = "g++";
        }

        datFile >> Settings::settings.width >> Settings::settings.height;

        if (Settings::settings.width < 800) {
            Settings::settings.width = 800;
        }
        if (Settings::settings.height < 600) {
            Settings::settings.height = 600;
        }

        datFile >> Settings::settings.terminalHidden;
    }

    App().run();
}
