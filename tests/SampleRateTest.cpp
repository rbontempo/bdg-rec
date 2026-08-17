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
        listener.fired = &stoppedFired;
        listener.out = &savedFile;
        engine->addListener(&listener);

        dest = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("bdg_ratetest");
        dest.deleteRecursively();
        dest.createDirectory();

        startMs = juce::Time::getMillisecondCounterHiRes();
        const bool started = engine->startRecording(dest);
        check(started, "startRecording() succeeded");

        if (! started) { setApplicationReturnValue(1); quit(); return; }

        // BDG_CHUNK_SECONDS is 2.0 for this build, so ~9s spans several rotations.
        startTimer(3000);
    }

    void timerCallback() override
    {
        stopTimer();

        if (phase == 0)
        {
            phase = 1;
            rateBefore = 0.0;
            if (auto* d = engine->getDeviceManager().getCurrentAudioDevice())
                rateBefore = d->getCurrentSampleRate();

            // Force the device to reopen at a different rate, exactly like a
            // hot-plug or a monitoring toggle would.
            auto setup = engine->getDeviceManager().getAudioDeviceSetup();
            setup.sampleRate = (rateBefore == 44100.0) ? 48000.0 : 44100.0;
            engine->getDeviceManager().setAudioDeviceSetup(setup, true);

            std::printf("     forced rate %.0f -> %.0f while recording\n",
                        rateBefore, setup.sampleRate);
            startTimer(3000);
            return;
        }

        double rateAfter = 0.0;
        if (auto* d = engine->getDeviceManager().getCurrentAudioDevice())
            rateAfter = d->getCurrentSampleRate();
        check(rateAfter != rateBefore, "device really did reopen at a new rate");

        check(! engine->isRecording(), "recording was stopped automatically");
        check(stoppedFired, "listener reported DeviceRateChanged");
        check(savedFile.existsAsFile(), "the salvaged take exists on disk");

        if (savedFile.existsAsFile())
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(savedFile));
            check(r != nullptr, "salvaged take is a readable WAV");
            if (r != nullptr)
            {
                const double secs = (double) r->lengthInSamples / r->sampleRate;
                std::printf("     saved: rate=%.0f  samples=%lld  duration=%.2fs\n",
                            r->sampleRate, (long long) r->lengthInSamples, secs);
                check(r->sampleRate == rateBefore,
                      "saved file is tagged with the rate it was RECORDED at");
                check(secs > 2.0 && secs < 4.5,
                      "duration matches real time (no pitch/length corruption)");
            }
        }

        auto orphans = dest.findChildFiles(juce::File::findDirectories, false, "BDG_rec_*");
        check(orphans.isEmpty(), "chunk folder cleaned up");

        std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                    failures, failures == 1 ? "" : "s");
        setApplicationReturnValue(failures == 0 ? 0 : 1);
        quit();
    }

    void shutdown() override { engine.reset(); }

private:
    struct L : AudioEngine::Listener
    {
        bool* fired; juce::File* out;
        void audioLevelsChanged(float, float) override {}
        void recordingFinished(const juce::File& f, AudioEngine::StopReason r) override
        {
            if (r == AudioEngine::StopReason::DeviceRateChanged) { *fired = true; *out = f; }
        }
    };
    L listener;
    bool stoppedFired = false;
    juce::File savedFile;
    int phase = 0;
    double rateBefore = 0.0;
    std::unique_ptr<AudioEngine> engine;
    juce::File dest;
    double startMs = 0.0;
};

START_JUCE_APPLICATION(Runner)
