// Renders the level widgets offscreen and inspects the pixels.
//
// These exist because three rounds of code review read this code and missed
// two real defects that are obvious the moment you look at the output: the
// waveform drawing raw linear amplitude (a flat dotted line for normal
// speech) and the "0" of the VU ruler being clipped at the right edge.
// Reading catches logic; only rendering catches scale and layout.
//
// No audio device and no display needed — CI runs these.
#include "../src/LevelScale.h"
#include "../src/VuMeter.h"
#include "../src/WaveformDisplay.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include <cmath>

static int failures = 0;

static void check(bool cond, const char* what)
{
    std::printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
    if (! cond) ++failures;
}

static float dbToRms(float db) { return std::pow(10.0f, db / 20.0f); }

// The VU meter draws on a transparent canvas, so alpha alone identifies it.
static bool isInk(juce::Colour c) { return c.getAlpha() > 40; }

// The waveform fills an opaque background first, so "has alpha" would match
// every pixel. A bar is anything far enough from that background colour.
static bool isBar(juce::Colour c)
{
    const int d = std::abs((int) c.getRed()   - 0x1e)
                + std::abs((int) c.getGreen() - 0x1e)
                + std::abs((int) c.getBlue()  - 0x20);
    return d > 60;   // the 6%-white baseline stays well under this
}

// ---------------------------------------------------------------- the scale

static void testScale()
{
    std::printf("\n-- LevelScale --\n");

    check(LevelScale::fromDb(-60.0f) == 0.0f, "-60 dB is the bottom of the scale");
    check(std::abs(LevelScale::fromDb(-36.0f) - 0.40f) < 0.001f, "-36 dB maps to 40%");
    check(std::abs(LevelScale::fromDb(-12.0f) - 0.80f) < 0.001f, "-12 dB maps to 80%");
    check(LevelScale::fromDb(0.0f) == 1.0f, "0 dB is the top of the scale");
    check(LevelScale::fromDb(-90.0f) == 0.0f, "below the floor clamps, does not go negative");
    check(LevelScale::fromRms(0.0f) == 0.0f, "silence maps to 0 without a log of zero");

    // The whole point: ordinary speech has to be visible. A linear scale puts
    // -36 dB at 1.6%, which is what made the waveform look dead.
    check(LevelScale::fromDb(-36.0f) > 10.0f * dbToRms(-36.0f),
          "the scale is far more generous than raw linear amplitude");
}

// ------------------------------------------------------------- the waveform

static void testWaveformHeight()
{
    std::printf("\n-- WaveformDisplay --\n");

    const int w = 300, h = 200;

    auto tallestBar = [&](float db)
    {
        WaveformDisplay wd;
        wd.setSize(w, h);
        wd.setRecording(true);
        for (int i = 0; i < 200; ++i)
            wd.pushRmsSample(dbToRms(db));

        juce::Image img(juce::Image::ARGB, w, h, true);
        juce::Graphics g(img);
        wd.paint(g);

        // Measure the drawn column nearest the right edge, where the newest
        // sample lands.
        int top = -1, bottom = -1;
        for (int y = 0; y < h; ++y)
        {
            if (isBar(img.getPixelAt(w - 2, y)))
            {
                if (top < 0) top = y;
                bottom = y;
            }
        }
        return (top < 0) ? 0 : (bottom - top + 1);
    };

    const int at36 = tallestBar(-36.0f);
    const int at12 = tallestBar(-12.0f);
    std::printf("     -36 dB -> %d px of %d;  -12 dB -> %d px\n", at36, h, at12);

    // Under the old linear scale this was 1.6% of the height and hit the 2px
    // floor. Anything in that range means the scale regressed.
    check(at36 > h / 5, "a -36 dB signal draws a clearly visible bar");
    check(at12 > at36, "a louder signal draws a taller bar");
    check(at12 <= (int) (h * 0.81f) + 2, "the tallest bar stays inside the widget");
}

// ----------------------------------------------------------- the VU ruler

static void testVuRulerFitsInside()
{
    std::printf("\n-- VuMeter ruler --\n");

    const int w = 390, h = 60;

    VuMeter vu;
    vu.setSize(w, h);
    vu.setLevels(0.02f, 0.02f);

    juce::Image img(juce::Image::ARGB, w, h, true);
    juce::Graphics g(img);
    vu.paint(g);

    // Rightmost column carrying any ink at all.
    int rightmost = -1;
    for (int x = w - 1; x >= 0 && rightmost < 0; --x)
        for (int y = 0; y < h; ++y)
            if (isInk(img.getPixelAt(x, y)))
            {
                rightmost = x;
                break;
            }

    std::printf("     rightmost drawn pixel: x=%d of %d\n", rightmost, w - 1);

    // "0" sits at the far right of the dB scale. Centred on its tick it used
    // to overflow the component and get sliced in half, which leaves ink in
    // the very last column. Clamped, the glyph ends a few pixels short.
    check(rightmost >= 0, "something was drawn");
    check(rightmost < w - 1, "the last label is not clipped by the right edge");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    testScale();
    testWaveformHeight();
    testVuRulerFitsInside();

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
