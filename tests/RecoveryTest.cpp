// Finding A5: after a crash the chunk that was being recorded still holds all
// its PCM, but its WAV header was never backfilled and claims zero samples.
// This builds exactly that file and checks recoverRecording() gets the audio.

#include "../src/AudioEngine.h"
#include <juce_events/juce_events.h>
#include <cstdio>

static int failures = 0;
static void check(bool c, const char* what)
{
    std::printf("%s  %s\n", c ? "PASS" : "FAIL", what);
    if (! c) ++failures;
}

// Zero out the `data` chunk size and the RIFF size, which is what a writer
// that never got destroyed leaves behind.
static bool truncateHeaderLengths(const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData(mb)) return false;

    auto* p = static_cast<juce::uint8*>(mb.getData());
    const size_t n = mb.getSize();
    bool patchedData = false;

    for (size_t i = 12; i + 8 <= n; )
    {
        const juce::uint32 size = (juce::uint32) (p[i+4] | (p[i+5] << 8) | (p[i+6] << 16) | ((juce::uint32) p[i+7] << 24));
        if (std::memcmp(p + i, "data", 4) == 0)
        {
            p[i+4] = p[i+5] = p[i+6] = p[i+7] = 0;  // data size = 0
            patchedData = true;
            break;
        }
        i += 8 + size + (size & 1);
    }

    p[4] = p[5] = p[6] = p[7] = 0; // RIFF size = 0

    return patchedData && f.replaceWithData(mb.getData(), mb.getSize());
}

class Runner : public juce::JUCEApplicationBase, private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "recovertest"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted(const juce::String&) override {}
    void systemRequestedQuit() override { quit(); }
    void suspended() override {}
    void resumed() override {}
    void unhandledException(const std::exception*, const juce::String&, int) override {}

    void initialise(const juce::String&) override
    {
        engine = std::make_unique<AudioEngine>();
        engine->initialise();

        dest = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("bdg_recovertest");
        dest.deleteRecursively();
        dest.createDirectory();

        engine->startRecording(dest);
        startTimer(5000);   // ~2 rotations at BDG_CHUNK_SECONDS=2
    }

    void timerCallback() override
    {
        stopTimer();
        engine->stopRecording();

        // stopRecording() cleaned up, so rebuild the crash scenario by hand:
        // take the finished take apart into a chunk folder and blank the
        // header of the last chunk, exactly as a crash would leave it.
        auto finished = dest.findChildFiles(juce::File::findFiles, false, "BDG_rec_*.wav");
        if (finished.isEmpty()) { check(false, "a recording was produced"); quit(); return; }

        juce::AudioFormatManager fm; fm.registerBasicFormats();
        juce::int64 fullLength = 0;
        double rate = 0;
        if (auto* r = fm.createReaderFor(finished[0]))
        {
            fullLength = r->lengthInSamples;
            rate = r->sampleRate;
            delete r;
        }

        auto folder = dest.getChildFile("BDG_rec_crashed");
        folder.createDirectory();
        finished[0].copyFileTo(folder.getChildFile("chunk_001.wav"));
        finished[0].deleteFile();

        auto chunk = folder.getChildFile("chunk_001.wav");
        const juce::int64 bytesOnDisk = chunk.getSize();
        check(truncateHeaderLengths(chunk), "built a chunk with a zeroed WAV header");

        // Prove the normal reader really does see nothing.
        juce::int64 asReaderSees = -1;
        if (auto* r = fm.createReaderFor(chunk))
        {
            asReaderSees = r->lengthInSamples;
            delete r;
        }
        std::printf("     chunk is %lld bytes on disk; the WAV reader %s\n",
                    (long long) bytesOnDisk,
                    asReaderSees < 0 ? "refuses to open it at all" : "reports 0 samples");
        check(asReaderSees <= 0,
              "a normal reader gets nothing from it (the bug's precondition)");

        auto recovered = engine->recoverRecording(folder);
        check(recovered.existsAsFile(), "recoverRecording() produced a file");

        if (recovered.existsAsFile())
        {
            std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(recovered));
            const juce::int64 got = (r != nullptr) ? r->lengthInSamples : 0;
            std::printf("     recovered %lld of %lld samples (%.1f%%), rate=%.0f\n",
                        (long long) got, (long long) fullLength,
                        fullLength > 0 ? 100.0 * (double) got / (double) fullLength : 0.0,
                        r != nullptr ? r->sampleRate : 0.0);

            check(got > 0, "the audio was actually recovered, not dropped");
            check(fullLength > 0 && got >= fullLength - 64,
                  "essentially every sample came back");
            check(r != nullptr && r->sampleRate == rate, "recovered at the original rate");
        }

        std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                    failures, failures == 1 ? "" : "s");
        quit();
    }

    void shutdown() override { engine.reset(); }

private:
    std::unique_ptr<AudioEngine> engine;
    juce::File dest;
};

START_JUCE_APPLICATION(Runner)
