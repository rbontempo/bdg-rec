#include "UpdateChecker.h"
#include <juce_gui_basics/juce_gui_basics.h>

UpdateChecker::UpdateChecker() : Thread("UpdateChecker") {}

UpdateChecker::~UpdateChecker()
{
    // Must exceed the worst case of the network call below (5 s connect plus
    // the read). At 3 s JUCE gave up waiting and killed the thread by force —
    // potentially inside the OS network stack, which is how you get the rare
    // unreproducible crash on quit.
    stopThread(15000);
}

void UpdateChecker::checkIfDue(juce::ApplicationProperties& props,
                                std::function<void(juce::String)> onUpdateAvailable)
{
    appProps = &props;
    callback = std::move(onUpdateAvailable);
    noUpdateCallback = {};   // the weekly check never answers "no update"

    if (auto* pf = props.getUserSettings())
    {
        // Stored as text, not an int: seconds since the epoch stop fitting in
        // a 32-bit int in 2038, and juce::String round-trips int64 cleanly.
        const auto last = pf->getValue("lastUpdateCheck", "0").getLargeIntValue();
        const auto now  = juce::Time::currentTimeMillis() / 1000;

        if (now - last < CHECK_INTERVAL_SECONDS)
            return; // too soon
    }

    startThread(juce::Thread::Priority::low);
}

void UpdateChecker::forceCheck(std::function<void(juce::String)> onUpdateAvailable,
                               std::function<void(bool)> onNoUpdate)
{
    if (isThreadRunning())
        return; // already checking

    // run() writes the timestamp through appProps. Today checkIfDue() always
    // runs first from the MainComponent constructor, so this cannot fire —
    // but nothing in the API says so, and the deref would be a null one.
    jassert(appProps != nullptr);
    if (appProps == nullptr)
        return;
    if (onUpdateAvailable)
        callback = std::move(onUpdateAvailable);
    noUpdateCallback = std::move(onNoUpdate);
    startThread(juce::Thread::Priority::low);
}

void UpdateChecker::reportNoUpdate(bool reachable)
{
    auto cb = std::exchange(noUpdateCallback, {});
    if (cb && ! threadShouldExit())
        juce::MessageManager::callAsync([cb, reachable]() { cb(reachable); });
}

void UpdateChecker::run()
{
    auto url = juce::URL("https://api.github.com/repos/rbontempo/bdg-rec/releases/latest");
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(5000)
                       .withExtraHeaders("Accept: application/vnd.github.v3+json")
                       // Returning false aborts the transfer, which lets the
                       // thread exit promptly on quit instead of being killed
                       // mid-call.
                       .withProgressCallback([this](int, int) { return ! threadShouldExit(); });

    auto stream = url.createInputStream(options);
    if (stream == nullptr || threadShouldExit())
    {
        reportNoUpdate(false); // silent for the weekly check, answered for the menu
        return;
    }

    auto response = stream->readEntireStreamAsString();
    if (threadShouldExit())
        return;
    auto json = juce::JSON::parse(response);

    auto tagName = json.getProperty("tag_name", "").toString();
    if (tagName.isEmpty())
    {
        reportNoUpdate(false);
        return;
    }

    // Strip leading 'v' if present
    if (tagName.startsWithChar('v') || tagName.startsWithChar('V'))
        tagName = tagName.substring(1);

    auto currentVersion = juce::String(JUCE_APPLICATION_VERSION_STRING);

    // Save timestamp regardless of result
    if (auto* pf = appProps->getUserSettings())
    {
        pf->setValue("lastUpdateCheck",
                     juce::String(juce::Time::currentTimeMillis() / 1000));
        pf->saveIfNeeded();
    }

    if (! isNewerVersion(tagName, currentVersion))
    {
        reportNoUpdate(true);
        return;
    }

    noUpdateCallback = {};
    if (callback)
    {
        auto ver = tagName;
        auto cb = callback;
        juce::MessageManager::callAsync([cb, ver]() {
            cb(ver);
        });
    }
}

bool UpdateChecker::isNewerVersion(const juce::String& remote, const juce::String& local)
{
    auto remParts = juce::StringArray::fromTokens(remote, ".", "");
    auto locParts = juce::StringArray::fromTokens(local,  ".", "");

    for (int i = 0; i < juce::jmax(remParts.size(), locParts.size()); ++i)
    {
        int r = (i < remParts.size()) ? remParts[i].getIntValue() : 0;
        int l = (i < locParts.size()) ? locParts[i].getIntValue() : 0;
        if (r > l) return true;
        if (r < l) return false;
    }
    return false; // equal
}
