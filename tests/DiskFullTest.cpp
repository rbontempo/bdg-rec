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

        dest = juce::File("/Volumes/BDGTEST").getChildFile("rec");
        dest.deleteRecursively();
        dest.createDirectory();

        startMs = juce::Time::getMillisecondCounterHiRes();
        const bool started = engine->startRecording(dest);
        check(started, "startRecording() succeeded");

        if (! started) { setApplicationReturnValue(1); quit(); return; }

        // BDG_CHUNK_SECONDS is 2.0 for this build, so ~9s spans several rotations.
        startTimer(6000);
    }

    void timerCallback() override
    {
        stopTimer();

        if (phase == 1) { reportAndQuit(); return; }

        auto chunkDirs = dest.findChildFiles(juce::File::findDirectories, false, "BDG_rec_*");
        check(chunkDirs.size() == 1, "chunk folder exists while recording");

        juce::int64 chunkBytes = 0;
        if (chunkDirs.size() == 1)
            for (auto& c : chunkDirs[0].findChildFiles(juce::File::findFiles, false, "chunk_*.wav"))
                chunkBytes += c.getSize();

        std::printf("     free before stop=%lld KB, chunk data=%lld KB\n",
                    (long long) dest.getBytesFreeOnVolume() / 1024,
                    (long long) chunkBytes / 1024);

        auto finalFile = engine->stopRecording();

        // The volume cannot hold a second copy, so concatenation must fail.
        check(finalFile == juce::File(),
              "stopRecording() reports failure instead of a bogus file");

        auto surviving = dest.findChildFiles(juce::File::findDirectories, false, "BDG_rec_*");
        check(surviving.size() == 1,
              "CHUNKS PRESERVED after failed concatenation (audio not destroyed)");

        juce::int64 survivingBytes = 0;
        if (surviving.size() == 1)
            for (auto& c : surviving[0].findChildFiles(juce::File::findFiles, false, "chunk_*.wav"))
                survivingBytes += c.getSize();
        std::printf("     surviving chunk data=%lld KB\n", (long long) survivingBytes / 1024);
        check(survivingBytes >= chunkBytes, "surviving chunks still hold the audio");

        auto stray = dest.findChildFiles(juce::File::findFiles, false, "BDG_rec_*.wav");
        check(stray.isEmpty(), "no truncated final file left behind to be mistaken for the take");

        // The failure notification is posted to the message thread, so give
        // the loop a turn before checking it.
        startTimer(300);
        phase = 1;
    }

    void reportAndQuit()
    {
        check(saveFailedFired, "listener was told the save failed");

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
    int phase = 0;
    std::unique_ptr<AudioEngine> engine;
    juce::File dest;
    double startMs = 0.0;
};

START_JUCE_APPLICATION(Runner)
