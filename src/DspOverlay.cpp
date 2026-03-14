#include "DspOverlay.h"

DspOverlay::DspOverlay()
{
    // Build the step list (all steps; some may be hidden)
    steps.add({ "Normalizacao",        true,  StepState::Pending });
    steps.add({ "Reducao de ruido",    true,  StepState::Pending });
    steps.add({ "Compressor",          true,  StepState::Pending });
    steps.add({ "Normalizacao final",  true,  StepState::Pending });
    steps.add({ "Salvando arquivo",    true,  StepState::Pending });

    setVisible(false);
    setInterceptsMouseClicks(true, true); // block all clicks behind
}

DspOverlay::~DspOverlay()
{
    stopTimer();
}

void DspOverlay::show(bool normalize, bool noiseReduction, bool compressor)
{
    // Reset steps
    for (auto& s : steps)
    {
        s.state   = StepState::Pending;
        s.enabled = true;
    }

    // steps[0] = "Normalizacao"       -> visible only if normalize
    // steps[1] = "Reducao de ruido"   -> visible only if noiseReduction
    // steps[2] = "Compressor"         -> visible only if compressor
    // steps[3] = "Normalizacao final" -> visible only if any treatment is on
    // steps[4] = "Salvando arquivo"   -> always visible
    steps.getReference(0).enabled = normalize;
    steps.getReference(1).enabled = noiseReduction;
    steps.getReference(2).enabled = compressor;
    steps.getReference(3).enabled = (normalize || noiseReduction || compressor);
    steps.getReference(4).enabled = true;

    spinAngle  = 0.0f;
    pulseAlpha = 1.0f;
    pulseDir   = -1.0f;

    setVisible(true);
    toFront(false);
    startTimerHz(60);
    repaint();
}

void DspOverlay::setCurrentStep(const juce::String& stepName)
{
    for (auto& s : steps)
    {
        if (s.state == StepState::Active)
            s.state = StepState::Done;
    }

    for (auto& s : steps)
    {
        if (s.name.equalsIgnoreCase(stepName) && s.enabled)
        {
            s.state = StepState::Active;
            break;
        }
    }

    juce::MessageManager::callAsync([this]() { repaint(); });
}

void DspOverlay::hide()
{
    stopTimer();
    setVisible(false);
}

void DspOverlay::timerCallback()
{
    spinAngle += 6.0f; // degrees per frame at 60fps
    if (spinAngle >= 360.0f) spinAngle -= 360.0f;

    // Pulse alpha for active step
    pulseAlpha += pulseDir * 0.05f;
    if (pulseAlpha <= 0.3f) { pulseAlpha = 0.3f; pulseDir =  1.0f; }
    if (pulseAlpha >= 1.0f) { pulseAlpha = 1.0f; pulseDir = -1.0f; }

    repaint();
}

void DspOverlay::paint(juce::Graphics& g)
{
    // Full overlay background
    g.fillAll(juce::Colour(0xF01A1A1C));

    auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();

    // Measure total content height to center vertically
    const float titleH      = 24.0f;
    const float subtitleH   = 20.0f;
    const float spinnerDiam = 48.0f + 6.0f; // diameter + stroke clearance
    const float stepsGap    = 14.0f;

    int enabledSteps = 0;
    for (const auto& s : steps)
        if (s.enabled) enabledSteps++;

    const float stepRowH  = 24.0f;
    const float totalH    = titleH + subtitleH + 12.0f + spinnerDiam + stepsGap
                            + (float)enabledSteps * stepRowH;

    float y = (bounds.getHeight() - totalH) * 0.5f;

    // --- "BDG REC" title ---
    {
        juce::Font boldFont(juce::FontOptions().withHeight(16.0f).withStyle("Bold"));
        g.setFont(boldFont);

        const juce::String bdgStr = "BDG";
        const juce::String recStr = " REC";

        // Approximate widths using glyph arrangement
        juce::GlyphArrangement ga;
        ga.addLineOfText(boldFont, bdgStr, 0.0f, 0.0f);
        const float bdgW = ga.getBoundingBox(0, -1, true).getWidth();
        ga.clear();
        ga.addLineOfText(boldFont, recStr, 0.0f, 0.0f);
        const float recW = ga.getBoundingBox(0, -1, true).getWidth();

        const float totalTitleW = bdgW + recW;
        float tx = cx - totalTitleW * 0.5f;

        g.setColour(BdgColours::primary);
        g.drawText(bdgStr,
                   juce::Rectangle<float>(tx, y, bdgW + 2.0f, titleH),
                   juce::Justification::centredLeft, false);

        g.setColour(juce::Colours::white);
        g.drawText(recStr,
                   juce::Rectangle<float>(tx + bdgW, y, recW + 4.0f, titleH),
                   juce::Justification::centredLeft, false);
    }
    y += titleH;

    // --- "Processando audio" ---
    {
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white.withAlpha(0.50f));
        g.drawText("Processando audio",
                   juce::Rectangle<float>(cx - 100.0f, y, 200.0f, subtitleH),
                   juce::Justification::centred, false);
    }
    y += subtitleH + 12.0f;

    // --- Spinning arc ---
    {
        const float radius   = 24.0f;
        const float strokeW  = 3.0f;
        const float arcDiam  = radius * 2.0f;
        const float arcY     = y;

        juce::Path arc;
        const float startRad = juce::degreesToRadians(spinAngle);
        const float sweepRad = juce::degreesToRadians(270.0f);
        arc.addCentredArc(cx, arcY + radius,
                          radius, radius,
                          0.0f,
                          startRad,
                          startRad + sweepRad,
                          true);

        g.setColour(BdgColours::primary);
        g.strokePath(arc, juce::PathStrokeType(strokeW,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));

        y += arcDiam + stepsGap;
    }

    // --- Step list ---
    const float stepIconX    = cx - 110.0f;
    const float stepLabelX   = stepIconX + 24.0f;
    const float stepLabelW   = 200.0f;

    for (const auto& s : steps)
    {
        if (!s.enabled) continue;

        const float rowCY = y + stepRowH * 0.5f;

        switch (s.state)
        {
            case StepState::Done:
            {
                // Pink filled circle
                const float r = 8.0f;
                g.setColour(BdgColours::primary);
                g.fillEllipse(stepIconX, rowCY - r, r * 2.0f, r * 2.0f);

                // White checkmark
                g.setColour(juce::Colours::white);
                juce::Path check;
                check.startNewSubPath(stepIconX + r - 4.0f, rowCY);
                check.lineTo(stepIconX + r - 1.0f, rowCY + 3.0f);
                check.lineTo(stepIconX + r + 4.0f, rowCY - 3.0f);
                g.strokePath(check, juce::PathStrokeType(1.5f,
                             juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));
                break;
            }
            case StepState::Active:
            {
                // Pulsing pink dot
                const float r = 6.0f;
                g.setColour(BdgColours::primary.withAlpha(pulseAlpha));
                g.fillEllipse(stepIconX + (8.0f - r), rowCY - r, r * 2.0f, r * 2.0f);
                break;
            }
            case StepState::Pending:
            {
                // Gray dot
                const float r = 4.0f;
                g.setColour(juce::Colours::white.withAlpha(0.20f));
                g.fillEllipse(stepIconX + (8.0f - r), rowCY - r, r * 2.0f, r * 2.0f);
                break;
            }
        }

        // Label
        float labelAlpha = (s.state == StepState::Active)  ? 1.0f
                         : (s.state == StepState::Done)    ? 0.70f
                                                           : 0.35f;
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white.withAlpha(labelAlpha));
        g.drawText(s.name,
                   juce::Rectangle<float>(stepLabelX, y, stepLabelW, stepRowH),
                   juce::Justification::centredLeft, false);

        y += stepRowH;
    }
}
