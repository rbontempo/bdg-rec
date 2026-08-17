// Smoke test for the chunk-rotation and concatenation path (findings A1 + A2).
// Records from the default input for a fixed wall-clock time with a short chunk
// length, then checks that the finished WAV holds every sample the chunks did.

#include "../src/AudioEngine.h"
#include <juce_events/juce_events.h>
#include <cstdio>

static int failures = 0;

static void check(bool cond, const char* what)
{
    std::printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
    if (! cond) ++failures;
}

class Runner : public juce::JUCEApplicationBase,
               private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "chunktest"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted(const juce::String&) override {}
    void systemRequestedQuit() override                 { quit(); }
    void suspended() override {}
    void resumed() override {}
    void unhandledException(const std::exception*, const juce::String&, int) override {}

    void initialise(const juce::String&) override
    {
        engine = std::make_unique<AudioEngine>();
        engine->initialise();
        listener.flag = &saveFailedFired;
        engine->addListener(&listener);

        dest = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("bdg_emptytest");
        dest.deleteRecursively();
        dest.createDirectory();

        startMs = juce::Time::getMillisecondCounterHiRes();
        const bool started = engine->startRecording(dest);
        check(started, "startRecording() succeeded");

        if (! started) { setApplicationReturnValue(1); quit(); return; }

        // BDG_CHUNK_SECONDS is 2.0 for this build, so ~9s spans several rotations.
        startTimer(30);
    }

    void timerCallback() override
    {
        stopTimer();
        auto f = engine->stopRecording();

        std::printf("     stopRecording() -> \"%s\"\n",
                    f == juce::File() ? "(empty)" : f.getFileName().toRawUTF8());

        check(! saveFailedFired,
              "an empty take is NOT reported to the user as a save failure");

        auto orphans = dest.findChildFiles(juce::File::findDirectories, false, "BDG_rec_*");
        check(orphans.isEmpty(),
              "no junk chunk folder left to trigger the recovery prompt next launch");

        std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                    failures, failures == 1 ? "" : "s");
        setApplicationReturnValue(failures == 0 ? 0 : 1);
        quit();
    }

    void shutdown() override { engine.reset(); }

private:
    struct L : AudioEngine::Listener
    {
        bool* flag;
        void audioLevelsChanged(float, float) override {}
        void recordingSaveFailed(const juce::File&) override { *flag = true; }
    };
    L listener;
    bool saveFailedFired = false;
    std::unique_ptr<AudioEngine> engine;
    juce::File dest;
    double startMs = 0.0;
};

START_JUCE_APPLICATION(Runner)
