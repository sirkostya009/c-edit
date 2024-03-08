#include <iostream>
#include "MyApp.h"

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

MyApp::MyApp() {
    ///
    /// Create our main App instance.
    ///
    app = App::Create();

    ///
    /// Create a resizable window by passing by OR'ing our window flags with
    /// kWindowFlags_Resizable.
    ///
    window = Window::Create(app->main_monitor(), WINDOW_WIDTH, WINDOW_HEIGHT,
                            false, kWindowFlags_Titled | kWindowFlags_Resizable);

    window->MoveToCenter();

    ///
    /// Create our HTML overlay-- we don't care about its initial size and
    /// position because it'll be calculated when we call OnResize() below.
    ///
    overlay = Overlay::Create(window, 1, 1, 0, 0);

    ///
    /// Force a call to OnResize to perform size/layout of our overlay.
    ///
    OnResize(window.get(), window->width(), window->height());

    ///
    /// Load a page into our overlay's View
    ///
    overlay->view()->LoadURL("file:///app.html");

    ///
    /// Register our MyApp instance as an AppListener so we can handle the
    /// App's OnUpdate event below.
    ///
    app->set_listener(this);

    ///
    /// Register our MyApp instance as a WindowListener so we can handle the
    /// Window's OnResize event below.
    ///
    window->set_listener(this);

    ///
    /// Register our MyApp instance as a LoadListener so we can handle the
    /// View's OnFinishLoading and OnDOMReady events below.
    ///
    overlay->view()->set_load_listener(this);

    ///
    /// Register our MyApp instance as a ViewListener so we can handle the
    /// View's OnChangeCursor and OnChangeTitle events below.
    ///
    overlay->view()->set_view_listener(this);
}

MyApp::~MyApp() = default;

void MyApp::Run() {
    app->Run();
}

void MyApp::OnUpdate() {
    ///
    /// This is called repeatedly from the application's update loop.
    ///
    /// You should update any app logic here.
    ///
}

void MyApp::OnClose(ultralight::Window *_) {
    app->Quit();
}

void MyApp::OnResize(ultralight::Window *_, uint32_t width, uint32_t height) {
    ///
    /// This is called whenever the window changes size (values in pixels).
    ///
    /// We resize our overlay here to take up the entire window.
    ///
    overlay->Resize(width, height);
}

void MyApp::OnFinishLoading(ultralight::View *caller,
                            uint64_t frame_id,
                            bool is_main_frame,
                            const String &url) {
    ///
    /// This is called when a frame finishes loading on the page.
    ///
}

void MyApp::OnDOMReady(ultralight::View *caller,
                       uint64_t frame_id,
                       bool is_main_frame,
                       const String &url) {
    ///
    /// This is called when a frame's DOM has finished loading on the page.
    ///
    /// This is the best time to setup any JavaScript bindings.
    ///
}

void MyApp::OnChangeCursor(ultralight::View *caller,
                           Cursor cursor) {
    ///
    /// This is called whenever the page requests to change the cursor.
    ///
    /// We update the main window's cursor here.
    ///
    window->SetCursor(cursor);
}

void MyApp::OnChangeTitle(ultralight::View *caller,
                          const String &title) {
    ///
    /// This is called whenever the page requests to change the title.
    ///
    /// We update the main window's title here.
    ///
    window->SetTitle(title.utf8().data());
}

void MyApp::OnAddConsoleMessage(ultralight::View *caller, MessageSource source, MessageLevel level,
                                const String &message, uint32_t line_number, uint32_t column_number,
                                const String &source_id) {
    std::cout << "Console: " << message.utf8().data() << std::endl;
}

bool MyApp::OnKeyEvent(const KeyEvent &evt) {
    std::cout << "Key Event: " << evt.virtual_key_code << ' ' << evt.native_key_code << std::endl;
    switch (evt.virtual_key_code) {
    case 27:
        app->Quit();
        return false;
    default:
        return true;
    }
}
