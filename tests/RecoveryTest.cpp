// Finding A5: after a crash the chunk that was being recorded still holds all
// its PCM, but its WAV header was never backfilled and claims zero samples.
// This builds exactly that file and checks recoverRecording() gets the audio.
//
// Also covered: a header that is merely stale (what the periodic flush leaves:
// the length at the last flush, not zero), and the take that is still being
// recorded never being offered as an orphan.

#include "../src/AudioEngine.h"
#include <juce_events/juce_events.h>
#include <cstdio>

static int failures = 0;
static void check(bool c, const char* what)
{
    std::printf("%s  %s\n", c ? "PASS" : "FAIL", what);
    if (! c) ++failures;
}

// Overwrite the `data` chunk size with `newSize` (0 = what a writer that never
// got destroyed leaves behind) and zero the RIFF size.
static bool patchDataSize(const juce::File& f, juce::uint32 newSize)
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
            const juce::uint32 v = (newSize == 0xffffffffu) ? (size / 2 / 3) * 3 : newSize;
            auto put = [p](size_t at, juce::uint32 x)
            {
                p[at]   = (juce::uint8) (x & 0xff);
                p[at+1] = (juce::uint8) ((x >> 8) & 0xff);
                p[at+2] = (juce::uint8) ((x >> 16) & 0xff);
                p[at+3] = (juce::uint8) ((x >> 24) & 0xff);
            };
            put(i + 4, v);

            // A crash leaves RIFF size 0; a periodic flush leaves a RIFF size
            // that is consistent with the (older) data size.
            put(4, v == 0 ? 0 : (juce::uint32) (i + 8 + v - 8));
            patchedData = true;
            break;
        }
        i += 8 + size + (size & 1);
    }

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

    static bool truncateHeaderLengths(const juce::File& f) { return patchDataSize(f, 0); }
    // Half of the real length, rounded down to whole samples.
    static bool staleHeaderLength(const juce::File& f)     { return patchDataSize(f, 0xffffffffu); }

    void timerCallback() override
    {
        stopTimer();

        // While the take is live, a scan of the same folder (what a second
        // launch does) must not offer it for recovery.
        {
            auto live = engine->findOrphanedRecordings(dest);
            check(live.isEmpty(), "the take being recorded is not offered as an orphan");
        }

        engine->stopRecording();

        // stopRecording() cleaned up, so rebuild the crash scenario by hand:
        // take the finished take apart into a chunk folder and blank the
        // header of the last chunk, exactly as a crash would leave it.
        auto finished = dest.findChildFiles(juce::File::findFiles, false, "BDG_rec_*.wav");
        if (finished.isEmpty()) { check(false, "a recording was produced");
                                 setApplicationReturnValue(1); quit(); return; }

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

        // ---- Stale header: claims about half of what is on disk ----
        if (recovered.existsAsFile())
        {
            auto folder2 = dest.getChildFile("BDG_rec_stale");
            folder2.createDirectory();
            auto chunk2 = folder2.getChildFile("chunk_001.wav");
            recovered.copyFileTo(chunk2);

            juce::int64 full = 0;
            if (auto* r = fm.createReaderFor(chunk2)) { full = r->lengthInSamples; delete r; }

            check(staleHeaderLength(chunk2), "built a chunk with a stale header length");

            juce::int64 headerSays = 0;
            if (auto* r = fm.createReaderFor(chunk2)) { headerSays = r->lengthInSamples; delete r; }
            std::printf("     stale header says %lld of %lld samples\n",
                        (long long) headerSays, (long long) full);

            recovered.deleteFile();
            auto again = engine->recoverRecording(folder2);
            juce::int64 got2 = 0;
            if (auto* r = fm.createReaderFor(again)) { got2 = r->lengthInSamples; delete r; }
            std::printf("     recovered %lld samples from the stale chunk\n", (long long) got2);
            check(headerSays > 0 && headerSays < full, "the stale header really is short");
            check(got2 >= full - 1, "every sample on disk came back, not just the header's count");
        }

        std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                    failures, failures == 1 ? "" : "s");
        setApplicationReturnValue(failures == 0 ? 0 : 1);
        quit();
    }

    void shutdown() override { engine.reset(); }

private:
    std::unique_ptr<AudioEngine> engine;
    juce::File dest;
};

START_JUCE_APPLICATION(Runner)
