#include "MainComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BdgRecApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "BDG rec"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }

    // One copy at a time. A second one scanned the recordings folder, found
    // the take the first copy was still writing, and offered to "recover" or
    // "discard" it — both of which deleted chunks out from under it.
    bool moreThanOneInstanceAllowed() override { return false; }

    void anotherInstanceStarted(const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront(true);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff1a1a1c), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(false, false);
            centreWithSize(720, 420);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(BdgRecApplication)
