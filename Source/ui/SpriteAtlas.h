#pragma once

// One sprite atlas — SPEC-005.
//
// A row-major grid of equally sized frames: frame 0 at top-left, filling left to
// right then wrapping. Grids rather than vertical filmstrips because a 100-frame
// knob as a filmstrip is 104 x 10400 px, past common maximum texture dimensions
// and unwieldy in every image editor. See ADR-002.

#include <JuceHeader.h>

namespace bv::ui
{

class SpriteAtlas
{
public:
    SpriteAtlas() = default;

    SpriteAtlas (juce::Image img, int numCols, int numRows,
                 int frameW, int frameH, int frameCount, double sweepDegrees)
        : image (std::move (img)), cols (numCols), rows (numRows),
          frameWidth (frameW), frameHeight (frameH),
          frames (frameCount), rotarySweepDegrees (sweepDegrees)
    {
    }

    /** True when the image is present AND its dimensions match the declared
        geometry. A sheet one pixel off misaligns every frame after the first, so
        it is rejected here rather than rendered wrong — SPEC-005 edge cases. */
    bool isValid() const
    {
        if (! image.isValid()) return false;
        if (cols <= 0 || rows <= 0 || frameWidth <= 0 || frameHeight <= 0) return false;
        if (frames <= 0 || frames > cols * rows) return false;

        // The image may be a @2x variant, so compare against the declared grid at
        // whatever integer scale it actually is.
        const int expectedW = cols * frameWidth;
        const int expectedH = rows * frameHeight;
        if (expectedW <= 0 || image.getWidth() % expectedW != 0) return false;

        const int scale = image.getWidth() / expectedW;
        return scale >= 1 && image.getHeight() == expectedH * scale;
    }

    /** Integer scale of the loaded sheet: 1 for a 1x asset, 2 for @2x. */
    int getScale() const
    {
        const int expectedW = cols * frameWidth;
        return (expectedW > 0 && image.isValid()) ? juce::jmax (1, image.getWidth() / expectedW) : 1;
    }

    /** Frame for a normalised [0, 1] value. SPEC-005 acceptance criterion 1:
        0.0 maps to 0 and 1.0 maps to frames - 1, with nothing out of range. */
    int frameIndexFor (double normalised) const
    {
        if (frames <= 1) return 0;
        const double clamped = juce::jlimit (0.0, 1.0, normalised);
        return (int) std::lround (clamped * (double) (frames - 1));
    }

    juce::Rectangle<int> frameRect (int index) const
    {
        const int scale = getScale();
        const int i = juce::jlimit (0, juce::jmax (0, frames - 1), index);
        return { (i % cols) * frameWidth * scale,
                 (i / cols) * frameHeight * scale,
                 frameWidth * scale,
                 frameHeight * scale };
    }

    void drawFrame (juce::Graphics& g, int index, juce::Rectangle<int> dest) const
    {
        if (! isValid()) return;

        // The integer source/dest overload, because Graphics::drawImage has no
        // form that takes both a target area and a source rectangle. Integer
        // coordinates also keep frame edges on pixel boundaries, so a frame
        // never bleeds a row of its neighbour.
        const auto src = frameRect (index);
        g.drawImage (image,
                     dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(),
                     src.getX(),  src.getY(),  src.getWidth(),  src.getHeight(),
                     false);
    }

    void drawFrameForValue (juce::Graphics& g, double normalised, juce::Rectangle<int> dest) const
    {
        drawFrame (g, frameIndexFor (normalised), dest);
    }

    int    getNumFrames()   const noexcept { return frames; }
    int    getFrameWidth()  const noexcept { return frameWidth; }
    int    getFrameHeight() const noexcept { return frameHeight; }
    double getSweepDegrees() const noexcept { return rotarySweepDegrees; }

private:
    juce::Image image;
    int cols = 0, rows = 0;
    int frameWidth = 0, frameHeight = 0;
    int frames = 0;
    double rotarySweepDegrees = 270.0;
};

} // namespace bv::ui
