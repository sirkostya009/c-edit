#ifndef C_EDIT_SETTINGS_H
#define C_EDIT_SETTINGS_H

#include <AppCore/AppCore.h>
#include <string>

namespace ul = ultralight;

class Settings : public ul::WindowListener, public ul::LoadListener, public ul::ViewListener {
    ul::RefPtr<ul::Window> window;
    ul::RefPtr<ul::Overlay> overlay;
    std::function<void()> closeCallback;

public:
    static struct settings_t {
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

        uint32_t settingsWidth;
        uint32_t settingsHeight;
    } settings;

    Settings(const ul::RefPtr<ul::App>& app, std::function<void()> onClose);

    void OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) override;
    void OnClose(ul::Window*) override;
    void OnResize(ul::Window*, uint32_t width, uint32_t height) override;
    void OnChangeTitle(ul::View *caller, const ul::String &title) override;
    bool OnKeyEvent(const ul::KeyEvent &evt) override;
};


#endif //C_EDIT_SETTINGS_H
