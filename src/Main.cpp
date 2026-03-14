#include "MainComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BdgRecApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "BDG REC"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff1a1a1c), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(800, 500, 2560, 1600);
            centreWithSize(960, 600);
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
