#include "Settings.h"

Settings::settings_t Settings::settings = {
        .lookUpCompiler = true,
        .compiler = "g++",
        .width = 800,
        .height = 600,

        .settingsWidth = 800,
        .settingsHeight = 650
};

Settings::Settings(const ul::RefPtr<ul::App> &app, std::function<void()> onClose)
: closeCallback{ std::move(onClose) }
, window{ ul::Window::Create(app->main_monitor(),settings.settingsWidth, settings.settingsHeight, false, ul::kWindowFlags_Resizable) }
, overlay{ ul::Overlay::Create(window, settings.settingsWidth, settings.settingsHeight, 0, 0) }
{
    window->set_listener(this);
    overlay->view()->set_load_listener(this);
    overlay->view()->set_view_listener(this);

    window->MoveToCenter();
    overlay->view()->LoadURL("file:///settings.html");
    overlay->Focus();
}

#define OnSettingsChange(name) \
    ul::JSCallback([] (const ul::JSObject &thisObject, const ul::JSArgs &args) { \
        if (!args.empty() && args[0].IsString()) { \
            settings.name = ((ul::String) args[0]).utf8().data(); \
        } \
    })

#define OnSettingsChangeBoolean(name) \
    ul::JSCallback([] (const ul::JSObject &thisObject, const ul::JSArgs &args) { \
        if (!args.empty() && args[0].IsBoolean()) { \
            settings.name = args[0].ToBoolean(); \
        } \
    })

void Settings::OnDOMReady(ul::View *caller, uint64_t frame_id, bool is_main_frame, const ul::String &url) {
    ul::SetJSContext(caller->LockJSContext()->ctx());
    auto global = ul::JSGlobalObject();

    global["OnBinChange"] = OnSettingsChange(compilerPath);
    global["OnLibChange"] = OnSettingsChange(libPath);
    global["OnFlagsChange"] = OnSettingsChange(flags);
    global["OnIncludeChange"] = OnSettingsChange(includePath);
    global["OnCompilerChange"] = OnSettingsChange(compiler);
    global["OnLookUpChange"] = OnSettingsChangeBoolean(lookUpCompiler);
    global["OnSaveTabsChange"] = OnSettingsChangeBoolean(saveTabs);

    ul::JSEval(("lookUpCompiler.checked = " + std::to_string(settings.lookUpCompiler)).c_str());
    ul::JSEval(("bin.value = '" + settings.compilerPath + '\'').c_str());
    ul::JSEval(("flags.value = '" + settings.flags + '\'').c_str());
    ul::JSEval(("include.value = '" + settings.includePath + '\'').c_str());
    ul::JSEval(("lib.value = '" + settings.libPath + '\'').c_str());
    ul::JSEval(("compiler.value = '" + settings.compiler + '\'').c_str());
}

void Settings::OnClose(ul::Window *) {
    settings.settingsWidth = window->width();
    settings.settingsHeight = window->height();
    window->Close();
    closeCallback();
}

void Settings::OnResize(ul::Window *, uint32_t width, uint32_t height) {
    overlay->Resize(width, height);
}

void Settings::OnChangeTitle(ul::View *caller, const ul::String &title) {
    window->SetTitle(title.utf8().data());
}

bool Settings::OnKeyEvent(const ul::KeyEvent &evt) {
    constexpr auto Escape = 27;

    switch (evt.virtual_key_code) {
    case Escape:
        OnClose(window.get());
    }
    return true;
}
