#ifndef MY_APP_H
#define MY_APP_H

#include <AppCore/AppCore.h>
#include <JavaScriptCore/JavaScript.h>

using namespace ultralight;

class MyApp : public AppListener,
              public WindowListener,
              public LoadListener,
              public ViewListener {
public:
    MyApp();

    ~MyApp() override;

    // Start the run loop.
    virtual void Run();

    // This is called continuously from the app's main loop.
    void OnUpdate() override;

    // This is called when the window is closing.
    void OnClose(ultralight::Window *window) override;

    // This is called whenever the window resizes.
    void OnResize(ultralight::Window *window, uint32_t width, uint32_t height) override;

    // This is called when the page finishes a load in one of its frames.
    void OnFinishLoading(ultralight::View *caller,
                         uint64_t frame_id,
                         bool is_main_frame,
                         const String &url) override;

    // This is called when the DOM has loaded in one of its frames.
    void OnDOMReady(ultralight::View *caller,
                    uint64_t frame_id,
                    bool is_main_frame,
                    const String &url) override;

    // This is called when the page requests to change the Cursor.
    void OnChangeCursor(ultralight::View *caller,
                        Cursor cursor) override;

    void OnChangeTitle(ultralight::View *caller,
                       const String &title) override;

    void OnAddConsoleMessage(ultralight::View *caller, MessageSource source, MessageLevel level, const String &message,
                             uint32_t line_number, uint32_t column_number, const String &source_id) override;

    bool OnKeyEvent(const KeyEvent &evt) override;

protected:
    RefPtr<App> app;
    RefPtr<Window> window;
    RefPtr<Overlay> overlay;
};

#endif
