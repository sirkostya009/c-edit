#include "App.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <shobjidl.h>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/process/windows.hpp>

namespace bf = boost::filesystem;
namespace ba = boost::asio;

namespace {
    COMDLG_FILTERSPEC filterSpec[] = {
            {L"C++ Files", L"*.cpp"},
            {L"C Files",   L"*.c"},
            {L"All Files", L"*.*"},
    };
}

App::App()
: app{ ul::App::Create() }
, window{ ul::Window::Create(app->main_monitor(),
          Settings::settings.width,
          Settings::settings.height,
          false,
          ul::kWindowFlags_Resizable | ul::kWindowFlags_Maximizable) }
, overlay{ ul::Overlay::Create(window, 1, 1, 0, 0) } {
    CoInitialize(nullptr);

    app->set_listener(this);
    window->set_listener(this);
    overlay->view()->set_load_listener(this);
    overlay->view()->set_view_listener(this);

    window->MoveToCenter();
    overlay->Resize(window->width(), window->height());
    overlay->view()->LoadURL("file:///app.html");
    overlay->Focus();
}

inline ul::String getContents() {
    return ul::JSEval("codearea.value");
}

inline void App::print(const std::string& message) {
    jsCallbacks.emplace("terminal.value += '" + message + '\'');
}

inline void App::status(const std::string &str) {
    jsCallbacks.emplace("runStatus.innerText = '" + str + '\'');
}

inline void App::toggleTerminal() {
    jsCallbacks.emplace("toggleTerminal()");
}

ul::String escapeTrailingSlash(const ul::String &str) {
    auto result = ul::String();
    auto utf8 = str.utf8();
    for (auto i = 0; i < utf8.size(); ++i) {
        if (utf8[i] == '\\') {
            result += "\\";
        }
        result += ul::String(utf8.data() + i, 1);
    }
    return result;
}

IFileDialog* createDialogInstance(bool save) {
    IFileDialog* dialog;
    auto hr = CoCreateInstance(
            save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_ALL,
            save ? IID_IFileSaveDialog : IID_IFileOpenDialog,
            reinterpret_cast<LPVOID *>(&dialog)
    );
    if (FAILED(hr)) {
        return nullptr;
    }
    dialog->SetFileTypes(3, filterSpec);

    return dialog;
}

ul::String saveDialog(HWND window) {
    auto dialog = createDialogInstance(true);

    if (SUCCEEDED(dialog->Show(window))) {
        IShellItem *item;
        if (FAILED(dialog->GetResult(&item))) {
            return {};
        }
        LPWSTR filePath;
        item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);

        CoTaskMemFree(filePath);
        item->Release();
        dialog->Release();

        auto wstr = std::wstring(filePath);
        auto path = std::string(wstr.begin(), wstr.end());

        return escapeTrailingSlash(path.c_str());
    }

    return {};
}

inline bool isActiveTab() {
    return ul::JSEval("activeTab").ToBoolean();
}

inline void clearTerminal() {
    ul::JSEval("terminal.value = ''");
}

inline bool isTerminalHidden() {
    return ul::JSEval("terminal.parentElement.classList.contains('hidden')").ToBoolean();
}

inline bool hasFilename() {
    return ul::JSEval("activeTab.getAttribute('data-filename') !== 'null'").ToBoolean();
}

inline ul::String getFilename() {
    return ul::JSEval("activeTab.getAttribute('data-filename')");
}

void App::SaveFile(const ul::JSObject&, const ul::JSArgs&) {
    if (ul::JSEval("!activeTab").ToBoolean()) {
        return;
    }

    if (!ul::JSEval("activeTab.getAttribute('data-filename') === 'null'").ToBoolean()) {
        auto filename = saveDialog((HWND) window->native_handle());
        if (filename.empty()) {
            return;
        }

        ul::JSEval("activeTab.setAttribute('data-filename', '" + filename + "')");
    }

    auto filename = (ul::String) ul::JSEval("activeTab.getAttribute('data-filename')");
    std::ofstream(filename.utf8().data()) << getContents().utf8().data();
}

void App::BuildAndRun(const ul::JSObject&, const ul::JSArgs&) {
    if (!isActiveTab()) {
        return;
    }

    if (isRunning) return;
    isRunning = true;

    clearTerminal();
    if (isTerminalHidden()) {
        toggleTerminal();
    }

    std::thread([&] {
        auto filename = hasFilename() ? std::string(getFilename().utf8().data()) : (bf::temp_directory_path() / bf::unique_path()).string() + ".cpp";
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
            print("Compilation failed\\n" + err.message());
            status("Failed");
            isRunning = false;
            return;
        }

        auto ioc = ba::io_context();
        in = new bp::async_pipe(ioc);
        out = bp::ipstream{};
        status("Running");
        auto child = bp::child(exe.string() + ".exe",
                               (bp::std_err & bp::std_out) > out,
                               bp::std_in < *in,
                               ioc, err, bp::windows::create_no_window);
        if (err) {
            print("Execution failed\\n" + err.message());
            status("Failed");
            delete in;
            in = nullptr;
            isRunning = false;
            return;
        }

        while (std::getline(out, read)) {
            if (!read.empty()) print(read);
        }

        child.wait();
        print("\\nProcess finished with exit code " + std::to_string(child.exit_code()));
        status("Finished");
        delete in;
        in = nullptr;
        isRunning = false;
    }).detach();
}

std::array<ul::String, 2> openDialog(HWND window) {
    auto dialog = createDialogInstance(false);

    if (SUCCEEDED(dialog->Show(window))) {
        IShellItem *item;
        if (FAILED(dialog->GetResult(&item))) {
            return {};
        }
        LPWSTR filePath;
        item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);

        CoTaskMemFree(filePath);
        item->Release();
        dialog->Release();

        auto wstr = std::wstring(filePath);
        auto path = std::string(wstr.begin(), wstr.end());

        auto file = std::ifstream(filePath);
        auto contents = std::string(std::istreambuf_iterator<char>(file), {});

        return {escapeTrailingSlash(path.c_str()), escapeTrailingSlash(contents.c_str())};
    }

    return {};
}

void App::OpenFile(const ul::JSObject&, const ul::JSArgs&) {
    auto [filename, contents] = openDialog((HWND) window->native_handle());

    if (filename.empty()) {
        return;
    }

    ul::JSEval("newTab('" + filename + "', `" + contents + "`)");
}

void App::Copy(const ul::JSObject&, const ul::JSArgs&) {
    auto text = ((ul::String) ul::JSEval("terminal.value")).utf8();
    auto hwnd = (HWND) window->native_handle();

    OpenClipboard(hwnd);
    EmptyClipboard();
    auto hg = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
    if (!hg) {
        CloseClipboard();
        return;
    }
    memcpy(GlobalLock(hg), text.data(), text.length() + 1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT,hg);
    CloseClipboard();
    GlobalFree(hg);
}

void App::OnStdIn(const ul::JSObject&, const ul::JSArgs& args) {
    if (in == nullptr) {
        return;
    }

    auto string = ((ul::String) args[0]).utf8();
    char key;

#define cmpstr(a, str) !strncmp(a.data(), str, a.length())

    if (string.length() > 1) {
        if (cmpstr(string, "Enter")) {
            key = '\n';
        } else if (cmpstr(string, "Tab")) {
            key = '\t';
        } else if (cmpstr(string, "Backspace")) {
            key = '\b';
        } else if (cmpstr(string, "Delete")) {
            key = 127;
        } else {
            return;
        }
    } else {
        key = string[0];
    }

    ba::async_write(*in, ba::buffer(&key, 1), [](const std::error_code &err, std::size_t) {
        if (err) std::cerr << "OnStdIn error" << std::endl;
    });
}

void App::OpenSettings(const ul::JSObject&, const ul::JSArgs&) {
    if (settings) {
        return;
    }

    settings = new Settings(app, [this] { closeSettings = true; });
}

void App::OnUpdate() {
    while (!jsCallbacks.empty()) {
        ul::JSEval(jsCallbacks.front().c_str());
        jsCallbacks.pop();
    }

    if (closeSettings) {
        delete settings;
        settings = nullptr;
        closeSettings = false;
        ul::SetJSContext(overlay->view()->LockJSContext()->ctx());
    }
}

bool App::OnKeyEvent(const ul::KeyEvent& evt) {
    if ((evt.modifiers & ul::KeyEvent::kMod_CtrlKey) && !evt.is_auto_repeat && evt.type == ul::KeyEvent::Type::kType_RawKeyDown) {
        std::cout << evt.virtual_key_code << std::endl;
        switch (evt.virtual_key_code) {
        case 'O': {
            auto [filename, contents] = openDialog((HWND) window->native_handle());

            if (filename.empty()) {
                break;
            }

            ul::JSEval("newTab('" + filename + "', `" + contents + "`)");
            break;
        }
        case 'S': {
            if (ul::JSEval("!activeTab").ToBoolean()) {
                return false;
            }

            if (ul::JSEval("activeTab.getAttribute('data-filename') === 'null'").ToBoolean()) {
                auto result = saveDialog((HWND) window->native_handle());

                if (result.empty()) {
                    return true;
                }

                ul::JSEval("activeTab.setAttribute('data-filename', '" + result + "')");
            }

            auto filename = (ul::String) ul::JSEval("activeTab.getAttribute('data-filename')");
            auto contents = (ul::String) ul::JSEval("codearea.value");

            std::ofstream(filename.utf8().data()) << contents.utf8().data();
            break;
        }
        case 'N':
            ul::JSEval("newTab()");

            break;
        case 'Q':
            OnClose(window.get());

            break;
        case 115: // F4
            ul::JSEval("activeTab && activeTab.children[2].click()");
            break;
        case 192: // ~
            ul::JSEval("toggleTerminal()");
            break;
        case 13: // Enter
            BuildAndRun({}, {});
            break;
        }
    }
    return true;
}

void App::OnClose(ul::Window *) {
    std::ofstream("settings.dat") << Settings::settings.lookUpCompiler << ' ' << Settings::settings.saveTabs << '\n'
                                  << Settings::settings.compilerPath << '\n'
                                  << Settings::settings.flags << '\n'
                                  << Settings::settings.includePath << '\n'
                                  << Settings::settings.libPath << '\n'
                                  << Settings::settings.compiler << '\n'
                                  << window->width() << ' ' << window->height() << '\n'
                                  << ul::JSEval("terminal.parentElement.classList.contains('hidden')").ToBoolean();

    std::exit(0);
}

void App::OnResize(ul::Window *, uint32_t width, uint32_t height) {
    overlay->Resize(width, height);
    overlay->view()->Resize(width, height);
}

void App::OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) {
    using ul::JSCallback, ul::JSCallbackWithRetval;
    ul::SetJSContext(caller->LockJSContext()->ctx());
    auto global = ul::JSGlobalObject();

    global["buildAndRun"] = BindJSCallback(&App::BuildAndRun);
    global["stdin"] = BindJSCallback(&App::OnStdIn);
    global["openSettings"] = BindJSCallback(&App::OpenSettings);
    global["saveDialog"] = BindJSCallback(&App::SaveFile);
    global["openFile"] = BindJSCallback(&App::OpenFile);
    global["stopRunning"] = JSCallback([this](const ul::JSObject&, const ul::JSArgs&) { OnStdIn({}, {"\x3"}); });
    global["copyTerminal"] = BindJSCallback(&App::Copy);

    if (Settings::settings.terminalHidden) {
        ul::JSEval("toggleTerminal()");
    }
}

void App::OnChangeCursor(ul::View *caller, ul::Cursor cursor) {
    window->SetCursor(cursor);
}

void App::OnChangeTitle(ul::View *caller, const ul::String &title) {
    window->SetTitle(title.utf8().data());
}

void App::OnAddConsoleMessage(ul::View *, ul::MessageSource, ul::MessageLevel lvl, const ul::String &message,
                              uint32_t line, uint32_t column, const ul::String &) {
    (lvl == ul::kMessageLevel_Error ? std::cerr : std::cout)
            << "Console: " << message.utf8().data() << " at line: " << line << ", column: " << column << std::endl;
}
