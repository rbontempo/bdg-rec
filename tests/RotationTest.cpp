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

        dest = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("bdg_chunktest");
        dest.deleteRecursively();
        dest.createDirectory();

        startMs = juce::Time::getMillisecondCounterHiRes();
        const bool started = engine->startRecording(dest);
        check(started, "startRecording() succeeded");

        if (! started) { quit(); return; }

        // BDG_CHUNK_SECONDS is 2.0 for this build, so ~9s spans several rotations.
        startTimer(9000);
    }

    void timerCallback() override
    {
        stopTimer();

        // Count the chunks before stopping — stopRecording() deletes them on success.
        auto chunkDirs = dest.findChildFiles(juce::File::findDirectories, false, "BDG_rec_*");
        check(chunkDirs.size() == 1, "exactly one chunk folder created");

        int chunkCount = 0;
        juce::int64 chunkSamples = 0;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        if (chunkDirs.size() == 1)
        {
            auto chunks = chunkDirs[0].findChildFiles(juce::File::findFiles, false, "chunk_*.wav");
            chunkCount = chunks.size();
            for (auto& c : chunks)
                if (auto* r = fm.createReaderFor(c))
                {
                    chunkSamples += r->lengthInSamples;
                    delete r;
                }
        }

        std::printf("     chunks=%d (partial pre-stop sample count=%lld)\n",
                    chunkCount, (long long) chunkSamples);
        check(chunkCount >= 3, "chunk rotation actually happened (>=3 chunks)");

        const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - startMs;
        auto finalFile = engine->stopRecording();

        check(finalFile.existsAsFile(), "stopRecording() returned an existing file");

        if (finalFile.existsAsFile())
        {
            std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(finalFile));
            check(r != nullptr, "final file is a readable WAV");

            if (r != nullptr)
            {
                std::printf("     final samples=%lld  rate=%.0f  bits=%d  ch=%u\n",
                            (long long) r->lengthInSamples, r->sampleRate,
                            (int) r->bitsPerSample, r->numChannels);

                // Real test for finding A2: with 4 rotations, the old
                // close-then-open path dropped tens to hundreds of ms each
                // time. Compare captured audio against the wall clock.
                const double expected = (elapsedMs / 1000.0) * r->sampleRate;
                const double deficitMs =
                    ((expected - (double) r->lengthInSamples) / r->sampleRate) * 1000.0;

                std::printf("     wall clock=%.0f ms  expected~%.0f samples  deficit=%.1f ms"
                            " across %d rotation(s)\n",
                            elapsedMs, expected, deficitMs, chunkCount - 1);

                check(deficitMs < 20.0,
                      "no audible gap lost across chunk boundaries (<20 ms total)");
                check(r->numChannels == 1, "final file is mono");
                check(r->bitsPerSample == 24, "final file is 24-bit");
            }
        }

        // On success the chunk folder must be gone; on failure it must survive.
        auto remaining = dest.findChildFiles(juce::File::findDirectories, false, "BDG_rec_*");
        check(remaining.isEmpty(), "chunk folder cleaned up after a verified save");

        std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                    failures, failures == 1 ? "" : "s");
        quit();
    }

    void shutdown() override { engine.reset(); }

private:
    std::unique_ptr<AudioEngine> engine;
    juce::File dest;
    double startMs = 0.0;
};

START_JUCE_APPLICATION(Runner)
