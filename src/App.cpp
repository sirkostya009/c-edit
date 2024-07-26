#include "App.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <shobjidl.h>
#include <boost/filesystem.hpp>

namespace bf = boost::filesystem;

namespace {
    COMDLG_FILTERSPEC filterSpec[] = {
            {L"C++ Files", L"*.cpp"},
            {L"C Files",   L"*.c"},
            {L"All Files", L"*.*"},
    };

    ul::JSFunction toggleTerminal;
    ul::JSFunction newTab;
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
    dialog->SetFileTypes(sizeof(filterSpec) / sizeof(COMDLG_FILTERSPEC), filterSpec);

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

std::pair<ul::String, ul::String> openDialog(HWND window) {
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

inline void writeFile(const char* filename) {
    std::ofstream(filename) << ((ul::String) ul::JSEval("codearea.value")).utf8().data();
}

void App::BuildAndRun(const ul::JSObject&, const ul::JSArgs&) {
    if (!ul::JSEval("activeTab").ToBoolean() || runner.IsRunning()) {
        return;
    }

    ul::JSEval("terminal.value = ''");
    if (ul::JSEval("terminal.parentElement.classList.contains('hidden')").ToBoolean()) {
        toggleTerminal({});
    } else {
        ul::JSEval("terminal.focus()");
    }

    auto filename = ul::JSEval("activeTab.getAttribute('data-filename') !== 'null'").ToBoolean()
                    ? std::string(((ul::String) ul::JSEval("activeTab.getAttribute('data-filename')")).utf8().data())
                    : (bf::temp_directory_path() / bf::unique_path()).string() + ".cpp";
    writeFile(filename.c_str());

    std::thread(
            &ProcessRunner::BuildAndRun,
            &runner,
            filename,
            [this](const std::string &message) { jsCallbacks.emplace("terminal.value += `" + message + '`'); },
            [this](const std::string &message) { jsCallbacks.emplace("runStatus.innerText = `" + message + '`'); }
    ).detach();
}

void App::OnStdIn(const ul::JSObject&, const ul::JSArgs& args) {
    if (!runner.IsRunning()) {
        return;
    }

    auto string = ((ul::String) args[0]).utf8();
#ifdef _DEBUG
    std::cout << string.data() << std::endl;
#endif
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
        } else if (cmpstr(string, "Unidentified")) {
            key = ' ';
        } else {
            return;
        }

        runner.Input(key);
    } else {
        for (size_t i = 0; i < string.sizeBytes(); ++i) {
            runner.Input(string.data()[i]);
        }
    }
}

void App::SaveFile(const ul::JSObject&, const ul::JSArgs&) {
    if (ul::JSEval("!activeTab").ToBoolean()) {
        return;
    }

    if (ul::JSEval("activeTab.getAttribute('data-filename') === 'null'").ToBoolean()) {
        auto filename = saveDialog((HWND) window->native_handle());
        if (filename.empty()) {
            return;
        }

        ul::JSEval(R"(activeTab.children[1].innerText = /.*([\/\\](.*))$/.exec(')"+filename+"')[2]");
        ul::JSEval("activeTab.setAttribute('data-filename', '" + filename + "')");
    }

    auto filename = (ul::String) ul::JSEval("activeTab.getAttribute('data-filename')");
    writeFile(filename.utf8().data());
}

void App::OpenSettings(const ul::JSObject&, const ul::JSArgs&) {
    if (settings) {
        return;
    }

    settings = std::make_unique<Settings>(app, [this] { closeSettings = true; });
}

void App::OpenFile(const ul::JSObject&, const ul::JSArgs&) {
    auto [filename, contents] = openDialog((HWND) window->native_handle());

    if (filename.empty()) {
        return;
    }

    newTab({filename, contents});
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

void App::OnUpdate() {
    while (!jsCallbacks.empty()) {
        ul::JSEval(jsCallbacks.front().c_str());
        jsCallbacks.pop();
    }

    if (closeSettings) {
        settings = nullptr;
        closeSettings = false;
        ul::SetJSContext(overlay->view()->LockJSContext()->ctx());
    }
}

bool App::OnKeyEvent(const ul::KeyEvent& evt) {
    if ((evt.modifiers & ul::KeyEvent::kMod_CtrlKey) && !evt.is_auto_repeat && evt.type == ul::KeyEvent::Type::kType_RawKeyDown) {
#ifdef _DEBUG
        std::cout << evt.virtual_key_code << std::endl;
#endif
        switch (evt.virtual_key_code) {
        case 'O':
            OpenFile({}, {});
            break;
        case 'S':
            SaveFile({}, {});
            break;
        case 'N':
            newTab({});
            break;
        case 'Q':
            OnClose(window.get());
            break;
        case 'C':
            runner.Input('\x3');
            break;
        case 115: // F4
            ul::JSEval("activeTab.children[2].click()");
            break;
        case 192: // ~
            toggleTerminal({});
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

    global["quit"] = JSCallback([this](const ul::JSObject&, const ul::JSArgs&) { OnClose(window.get()); });
    global["buildAndRun"] = BindJSCallback(&App::BuildAndRun);
    global["stdin"] = BindJSCallback(&App::OnStdIn);
    global["openSettings"] = BindJSCallback(&App::OpenSettings);
    global["saveDialog"] = BindJSCallback(&App::SaveFile);
    global["openFile"] = BindJSCallback(&App::OpenFile);
    global["stopRunning"] = JSCallback([this](const ul::JSObject&, const ul::JSArgs&) { runner.Input('\x3'); });
    global["copyTerminal"] = BindJSCallback(&App::Copy);

    toggleTerminal = global["toggleTerminal"];
    newTab = global["newTab"];

    if (Settings::settings.terminalHidden) {
        toggleTerminal({});
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
