#ifndef C_EDIT_APP_H
#define C_EDIT_APP_H

#include <AppCore/AppCore.h>
#include <string>
#include <queue>
#include "Settings.h"
#include "ProcessRunner.h"

namespace ul = ultralight;

class App : public ul::AppListener, public ul::WindowListener, public ul::LoadListener, public ul::ViewListener {
public:
    App();
    inline void run() { app->Run(); }
    void OnUpdate() override;
    bool OnKeyEvent(const ul::KeyEvent &evt) override;
    void OnClose(ul::Window*) override;
    void OnResize(ul::Window*, uint32_t width, uint32_t height) override;
    void OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) override;
    void OnChangeCursor(ul::View *caller, ul::Cursor cursor) override;
    void OnChangeTitle(ul::View *caller, const ul::String &title) override;
    void OnAddConsoleMessage(ul::View*, ul::MessageSource, ul::MessageLevel lvl, const ul::String &message, uint32_t line, uint32_t column, const ul::String &) override;
private:
    ul::RefPtr<ul::App> app;
    ul::RefPtr<ul::Window> window;
    ul::RefPtr<ul::Overlay> overlay;

    std::queue<std::string> jsCallbacks;
    ProcessRunner runner;

    std::unique_ptr<Settings> settings;
    bool closeSettings = false;

    void BuildAndRun(const ul::JSObject&, const ul::JSArgs&);
    void OnStdIn(const ul::JSObject&, const ul::JSArgs& args);
    void SaveFile(const ul::JSObject&, const ul::JSArgs&);
    void OpenSettings(const ul::JSObject&, const ul::JSArgs&);
    void OpenFile(const ul::JSObject&, const ul::JSArgs &);
    void Copy(const ul::JSObject&, const ul::JSArgs&);
};

#endif //C_EDIT_APP_H
