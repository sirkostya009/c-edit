#include <AppCore/AppCore.h>
#include <iostream>
#include <boost/process.hpp>
#include <fstream>
#include <shobjidl.h>

namespace ul = ultralight;
namespace bp = boost::process;
namespace bf = boost::filesystem;
namespace ba = boost::asio;

class App : public ul::AppListener, public ul::WindowListener, public ul::LoadListener, public ul::ViewListener {
    ul::RefPtr<ul::App> app;
    ul::RefPtr<ul::Window> window;
    ul::RefPtr<ul::Overlay> overlay;

    ul::JSFunction showContents;
    ul::JSFunction getContents;
    ul::JSFunction addTerminal;
    ul::JSFunction getFilepath;
    ul::JSFunction addTab;
    ul::JSFunction clearTerminal;

    ba::streambuf in;
public:
    App()
    : app{ ul::App::Create() }
    , window{ ul::Window::Create(app->main_monitor(), 800, 600, false, ul::kWindowFlags_Resizable | ul::kWindowFlags_Maximizable) }
    , overlay{ ul::Overlay::Create(window, 1, 1, 0, 0) }
    {
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

    void run() {
        app->Run();
    }

    void OnClose(ul::Window*) override {
        std::exit(0);
    }

    void OnResize(ul::Window*, uint32_t width, uint32_t height) override {
        overlay->Resize(width, height);
    }

    void OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) override {
        using ul::JSCallback, ul::JSCallbackWithRetval;
        auto jsContextPtr = caller->LockJSContext();
        ul::SetJSContext(jsContextPtr->ctx());
        auto global = ul::JSGlobalObject();

        global["OnRun"]          = BindJSCallback(&App::OnRun);
        global["OnStdIn"]        = BindJSCallback(&App::OnStdIn);
        global["SaveFile"]       = BindJSCallback(&App::SaveFile);
        global["OpenFileDialog"] = BindJSCallbackWithRetval(&App::OpenFileDialog);
        global["SaveFileDialog"] = BindJSCallbackWithRetval(&App::SaveFileDialog);

        clearTerminal = global["clearTerminal"];
        showContents  = global["showContents"];
        getContents   = global["getContents"];
        addTerminal   = global["addTerminal"];
        getFilepath   = global["getFilepath"];
        addTab        = global["addTab"];
    }

    void OnChangeCursor(ul::View *caller, ul::Cursor cursor) override {
        window->SetCursor(cursor);
    }

    void OnChangeTitle(ul::View *caller, const ul::String &title) override {
        window->SetTitle(title.utf8().data());
    }

    void OnAddConsoleMessage(ul::View *caller, ul::MessageSource source, ul::MessageLevel level,
                             const ul::String &message, uint32_t line, uint32_t column,
                             const ul::String &source_id) override {
        std::cout << "Console: " << message.utf8().data() << " at line: " << line << ", column: " << column << std::endl;
    }

    void OnRun(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        auto filename = std::string();

        if (args[0].IsString()) {
            filename = ((ul::String) args[0].ToString()).utf8().data();
        } else {
            filename = std::move((bf::temp_directory_path() / bf::unique_path()).string() + ".cpp");
            std::ofstream(filename) << ((ul::String) getContents({})).utf8().data();
        }

        try {
            clearTerminal({});
            auto buf = bp::ipstream();
            auto out = bf::temp_directory_path() / bf::unique_path();
            auto compilation = bp::child(bp::search_path("g++"), filename, "-o", out, (bp::std_err & bp::std_out) > buf);

            for (auto bufstr = std::string(); compilation.running() && std::getline(buf, bufstr);) {
                addTerminal({bufstr.c_str()});
            }
            compilation.wait();

            auto ioc = ba::io_context();
            auto outbuf = ba::streambuf();
            in.consume(in.size());
            auto child = bp::child(out.string() + ".exe", (bp::std_err & bp::std_out) > outbuf, bp::std_in < in, ioc);

            ioc.run();
        } catch (const std::exception &e) {
            addTerminal({e.what()});
        }
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

        auto file = std::fstream(filePath);
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

        std::ofstream(filePath) << ((ul::String) getContents({})).utf8().data();

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
        return strncmp(a.data(), str, a.length());
    }

    void OnStdIn(const ul::JSObject &thisObject, const ul::JSArgs &args) {
        if (args.empty() || !args[0].IsString()) {
            std::cout << "OnStdIn invalid args" << std::endl;
            return;
        }

        auto string = ((ul::String) args[0]).utf8();
        char key;

        if (string.length() > 1) {
            // account for enter, tab, etc
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

        std::cout << key << std::endl;
    }
};

auto main() -> int {
    App().run();
}
