#include "PluginEditor.h"
#include <cmath>

void AnalogKnobLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, const float rotaryStartAngle,
                                     const float rotaryEndAngle, juce::Slider&)
{
    const float radius = juce::jmin(width / 2, height / 2) - 8.0f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Marcas radiales estilo consola vintage
    const int numTicks = 11;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float r1 = radius + 3.0f;
        const float r2 = radius + (i % 5 == 0 ? 7.5f : 5.0f);

        g.setColour(t <= sliderPos ? juce::Colour(0xFFD4A359) : juce::Colour(0xFF5A554E));
        g.drawLine(cx + std::sin(a) * r1, cy - std::cos(a) * r1,
                   cx + std::sin(a) * r2, cy - std::cos(a) * r2, 1.5f);
    }

    // Sombra de la perilla
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - radius + 2.0f, cy - radius + 4.0f, radius * 2.0f, radius * 2.0f);

    // Cuerpo metálico oscuro con degradado
    juce::ColourGradient knobGrad(juce::Colour(0xFF3E3D42), cx - radius * 0.4f, cy - radius * 0.5f,
                                  juce::Colour(0xFF151518), cx + radius * 0.5f, cy + radius * 0.6f, true);
    g.setGradientFill(knobGrad);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Borde metálico biselado
    g.setColour(juce::Colour(0xFF6B655D));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // Tapa interior
    const float innerR = radius * 0.72f;
    g.setColour(juce::Colour(0xFF222126));
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    // Indicador dorado
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.8f, -radius + 4.0f, 3.6f, radius * 0.55f, 1.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
    g.setColour(juce::Colour(0xFFE6B972));
    g.fillPath(pointer);
}

DeReverbenjaminEditor::DeReverbenjaminEditor(DeReverbenjaminProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&analogLAF);

    setupKnob(roomAmountSlider, roomAmountLabel, "DE-ROOM");
    setupKnob(focusSlider, focusLabel, "AMBIANCE");
    setupKnob(decaySlider, decayLabel, "DECAY");
    setupKnob(smoothSlider, smoothLabel, "SMOOTH");

    setupKnob(tailAmountSlider, tailAmountLabel, "DE-TAIL");
    setupKnob(sensSlider, sensLabel, "SENSITIVITY");
    setupKnob(releaseSlider, releaseLabel, "RELEASE", " ms");
    setupKnob(outputSlider, outputLabel, "OUTPUT", " dB");

    roomAmountAtt = std::make_unique<Attachment>(audioProcessor.apvts, "roomAmount", roomAmountSlider);
    focusAtt      = std::make_unique<Attachment>(audioProcessor.apvts, "focus", focusSlider);
    decayAtt      = std::make_unique<Attachment>(audioProcessor.apvts, "decay", decaySlider);
    smoothAtt     = std::make_unique<Attachment>(audioProcessor.apvts, "smooth", smoothSlider);

    tailAmountAtt = std::make_unique<Attachment>(audioProcessor.apvts, "tailAmount", tailAmountSlider);
    sensAtt       = std::make_unique<Attachment>(audioProcessor.apvts, "sensitivity", sensSlider);
    releaseAtt    = std::make_unique<Attachment>(audioProcessor.apvts, "release", releaseSlider);
    outputAtt     = std::make_unique<Attachment>(audioProcessor.apvts, "output", outputSlider);

    setSize(620, 380);
    startTimerHz(30);
}

DeReverbenjaminEditor::~DeReverbenjaminEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void DeReverbenjaminEditor::setupKnob(juce::Slider& slider, juce::Label& label,
                                      const juce::String& name, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFD8CCB8));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF121215));
    if (suffix.isNotEmpty())
        slider.setTextValueSuffix(suffix);
    addAndMakeVisible(slider);

    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xFFD4A359));
    label.setFont(11.5f);
    addAndMakeVisible(label);
}

void DeReverbenjaminEditor::timerCallback()
{
    const float target = audioProcessor.currentGainReduction.load();
    smoothedGr = 0.72f * smoothedGr + 0.28f * target;
    repaint(380, 14, 215, 76);
}

void DeReverbenjaminEditor::drawScrew(juce::Graphics& g, float cx, float cy) const
{
    const float r = 6.0f;
    g.setColour(juce::Colour(0xFF3A393E));
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colour(0xFF141416));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.2f);
    g.drawLine(cx - 3.8f, cy - 2.2f, cx + 3.8f, cy + 2.2f, 1.4f);
}

void DeReverbenjaminEditor::drawVuMeter(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Marco oscuro empotrado
    g.setColour(juce::Colour(0xFF0E0E10));
    g.fillRoundedRectangle(area.expanded(3.0f), 6.0f);

    // Fondo retroiluminado cálido estilo VU análogo
    juce::ColourGradient vuGrad(juce::Colour(0xFFF6E7C1), area.getCentreX(), area.getY(),
                                juce::Colour(0xFFD6B97B), area.getCentreX(), area.getBottom(), false);
    g.setGradientFill(vuGrad);
    g.fillRoundedRectangle(area, 4.0f);

    // Escala de reducción (0 a -18 dB)
    const float pivotX = area.getCentreX();
    const float pivotY = area.getBottom() + 18.0f;
    const float arcRadius = area.getHeight() + 6.0f;

    g.setColour(juce::Colour(0xFF2B2318));
    g.setFont(9.5f);
    g.drawText("GAIN REDUCTION", area.withTrimmedTop(4.0f).withHeight(14.0f), juce::Justification::centred);

    const float startAngle = -0.68f; // izquierda (-18 dB)
    const float endAngle   =  0.68f; // derecha (0 dB)

    for (int i = 0; i <= 6; ++i)
    {
        const float t = static_cast<float>(i) / 6.0f;
        const float a = startAngle + t * (endAngle - startAngle);
        g.setColour(i < 2 ? juce::Colour(0xFFB33224) : juce::Colour(0xFF2B2318));
        g.drawLine(pivotX + std::sin(a) * (arcRadius - 8.0f), pivotY - std::cos(a) * (arcRadius - 8.0f),
                   pivotX + std::sin(a) * arcRadius,          pivotY - std::cos(a) * arcRadius, 1.4f);
    }

    // Aguja del medidor
    const float grDb = juce::jlimit(-18.0f, 0.0f, juce::Decibels::gainToDecibels(smoothedGr, -18.0f));
    const float norm = (grDb + 18.0f) / 18.0f; // 1.0 = 0 dB (derecha), 0.0 = -18 dB (izquierda)
    const float needleAngle = startAngle + norm * (endAngle - startAngle);

    g.saveState();
    g.reduceClipRegion(area.toNearestInt());
    g.setColour(juce::Colour(0xFF1A1612));
    g.drawLine(pivotX, pivotY,
               pivotX + std::sin(needleAngle) * (arcRadius + 2.0f),
               pivotY - std::cos(needleAngle) * (arcRadius + 2.0f), 2.0f);
    g.restoreState();

    // Borde de cristal
    g.setColour(juce::Colour(0xFF4A443C));
    g.drawRoundedRectangle(area, 4.0f, 1.5f);
}

void DeReverbenjaminEditor::paint(juce::Graphics& g)
{
    // Chasis metálico cepillado oscuro
    juce::ColourGradient bgGrad(juce::Colour(0xFF26262B), 0.0f, 0.0f,
                                juce::Colour(0xFF141417), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Marco metálico exterior
    g.setColour(juce::Colour(0xFF3E3B36));
    g.drawRect(getLocalBounds().reduced(2), 2);

    // Tornillos en las 4 esquinas
    drawScrew(g, 16.0f, 16.0f);
    drawScrew(g, static_cast<float>(getWidth()) - 16.0f, 16.0f);
    drawScrew(g, 16.0f, static_cast<float>(getHeight()) - 16.0f);
    drawScrew(g, static_cast<float>(getWidth()) - 16.0f, static_cast<float>(getHeight()) - 16.0f);

    // Logo / Título estilo hardware vintage
    g.setColour(juce::Colour(0xFFEAE0D0));
    g.setFont(24.0f);
    g.drawText("DeReverbenjamin", 34, 22, 320, 30, juce::Justification::left);

    g.setColour(juce::Colour(0xFFD4A359));
    g.setFont(11.0f);
    g.drawText("HYBRID SPECTRAL DE-ROOM & TAIL PROCESSOR", 36, 52, 320, 16, juce::Justification::left);

    // Medidor VU análogo
    drawVuMeter(g, juce::Rectangle<float>(390.0f, 18.0f, 196.0f, 64.0f));

    // Paneles empotrados para Etapa 1 y Etapa 2
    auto drawPanel = [&](juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(juce::Colour(0xFF1A1A1E));
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
        g.setColour(juce::Colour(0xFF3A352E));
        g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.2f);

        g.setColour(juce::Colour(0xFFA89C8A));
        g.setFont(10.5f);
        g.drawText(title, bounds.getX() + 12, bounds.getY() + 6, bounds.getWidth() - 24, 16, juce::Justification::left);
    };

    drawPanel({ 24, 96, 572, 128 }, "STAGE 1  //  SPECTRAL DE-ROOM & AMBIANCE");
    drawPanel({ 24, 236, 572, 128 }, "STAGE 2  //  TAIL DE-REVERB & OUTPUT");
}

void DeReverbenjaminEditor::resized()
{
    auto layoutRow = [](int y, std::array<juce::Slider*, 4> sliders, std::array<juce::Label*, 4> labels)
    {
        const int startX = 44;
        const int spacing = 138;
        const int knobW = 114;
        const int knobH = 82;

        for (size_t i = 0; i < 4; ++i)
        {
            const int x = startX + static_cast<int>(i) * spacing;
            labels[i]->setBounds(x, y, knobW, 16);
            sliders[i]->setBounds(x, y + 16, knobW, knobH);
        }
    };

    layoutRow(118,
              { &roomAmountSlider, &focusSlider, &decaySlider, &smoothSlider },
              { &roomAmountLabel,  &focusLabel,  &decayLabel,  &smoothLabel });

    layoutRow(258,
              { &tailAmountSlider, &sensSlider, &releaseSlider, &outputSlider },
              { &tailAmountLabel,  &sensLabel,  &releaseLabel,  &outputLabel });
}
