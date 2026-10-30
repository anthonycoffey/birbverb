#pragma once

// Loads sprites.json and the atlases it declares — SPEC-005.
//
// Everything here degrades. No manifest, no atlas, a corrupt PNG, or a sheet
// whose dimensions disagree with the manifest: the affected controls fall back to
// procedural drawing and the rest keep their art. SPEC-005 requirement 6 and
// acceptance criterion 4.
//
// That is not defensive padding. It is what lets the DSP and the art proceed in
// parallel: the plugin is fully usable with zero sheets committed.

#include <JuceHeader.h>

#include "SpriteAtlas.h"

#include <map>
#include <string>

#if BIRBVERB_HAS_ASSETS
 #include "BinaryData.h"
#endif

namespace bv::ui
{

class SpriteStore
{
public:
    SpriteStore() { load(); }

    /** nullptr when this control has no usable art — draw the fallback. */
    const SpriteAtlas* atlasForControl (const juce::String& controlId) const
    {
        if (auto* name = controlAtlas.getVarPointer (controlId))
            return atlas (name->toString());

        return nullptr;
    }

    const SpriteAtlas* atlas (const juce::String& atlasName) const
    {
        const auto it = atlases.find (atlasName.toStdString());
        if (it == atlases.end()) return nullptr;
        return it->second.isValid() ? &it->second : nullptr;
    }

    /** Top-left of the control's frame rectangle in the 600 x 650 editor space,
        from the manifest. Falls back to the caller's default when absent. */
    juce::Point<int> controlPosition (const juce::String& controlId,
                                      juce::Point<int> fallback) const
    {
        if (auto* p = controlPos.getVarPointer (controlId))
        {
            auto* arr = p->getArray();
            if (arr != nullptr && arr->size() == 2)
                return { (int) (*arr)[0], (int) (*arr)[1] };
        }
        return fallback;
    }

    bool hasAnyArt() const { return anyValid; }

private:
    //==========================================================================
    static juce::MemoryBlock loadBinaryAsset (const juce::String& filename)
    {
       #if BIRBVERB_HAS_ASSETS
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (filename == BinaryData::originalFilenames[i])
            {
                int size = 0;
                if (auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                    return { data, (size_t) size };
            }
        }
       #else
        juce::ignoreUnused (filename);
       #endif
        return {};
    }

    static juce::Image decode (const juce::MemoryBlock& block)
    {
        if (block.getSize() == 0) return {};
        return juce::ImageFileFormat::loadFrom (block.getData(), block.getSize());
    }

    /** @2x when the display is scaled, 1x otherwise, falling back to whichever
        exists — SPEC-005 requirement 4. */
    static juce::Image loadImageFor (const juce::String& filename)
    {
        const auto scale = juce::Desktop::getInstance().getDisplays()
                               .getPrimaryDisplay() != nullptr
                         ? juce::Desktop::getInstance().getDisplays()
                               .getPrimaryDisplay()->scale
                         : 1.0;

        const auto stem = filename.upToLastOccurrenceOf (".", false, false);
        const auto ext  = filename.fromLastOccurrenceOf (".", true, false);
        const auto hiDpiName = stem + "@2x" + ext;

        if (scale > 1.0)
        {
            auto hi = decode (loadBinaryAsset (hiDpiName));
            if (hi.isValid()) return hi;
        }

        auto base = decode (loadBinaryAsset (filename));
        if (base.isValid()) return base;

        // 1x absent but @2x present: use it rather than drawing nothing.
        return decode (loadBinaryAsset (hiDpiName));
    }

    void load()
    {
        auto manifestBlock = loadBinaryAsset ("sprites.json");
        if (manifestBlock.getSize() == 0)
            return;   // no manifest: everything falls back, which is a valid state

        const auto parsed = juce::JSON::parse (manifestBlock.toString());
        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
        {
            // A malformed manifest triggers the fallback for every control rather
            // than failing to open the editor — SPEC-005 edge cases.
            jassertfalse;
            return;
        }

        if (auto* atlasObj = root->getProperty ("atlases").getDynamicObject())
        {
            for (const auto& entry : atlasObj->getProperties())
            {
                auto* spec = entry.value.getDynamicObject();
                if (spec == nullptr) continue;

                const auto file   = spec->getProperty ("file").toString();
                const int  cols   = (int) spec->getProperty ("cols");
                const int  rows   = (int) spec->getProperty ("rows");
                const int  fw     = (int) spec->getProperty ("frameWidth");
                const int  fh     = (int) spec->getProperty ("frameHeight");
                const int  frames = (int) spec->getProperty ("frames");
                const double sweep = spec->hasProperty ("rotarySweepDegrees")
                                   ? (double) spec->getProperty ("rotarySweepDegrees")
                                   : 270.0;

                SpriteAtlas atlas { loadImageFor (file), cols, rows, fw, fh, frames, sweep };
                if (atlas.isValid())
                    anyValid = true;

                atlases.emplace (entry.name.toString().toStdString(), std::move (atlas));
            }
        }

        if (auto* controls = root->getProperty ("controls").getDynamicObject())
        {
            for (const auto& entry : controls->getProperties())
            {
                auto* spec = entry.value.getDynamicObject();
                if (spec == nullptr) continue;

                const auto id = entry.name.toString();
                controlAtlas.set (id, spec->getProperty ("atlas"));

                juce::Array<juce::var> pos;
                pos.add (spec->getProperty ("x"));
                pos.add (spec->getProperty ("y"));
                controlPos.set (id, pos);
            }
        }
    }

    std::map<std::string, SpriteAtlas> atlases;
    juce::NamedValueSet controlAtlas;   ///< control id -> atlas name
    juce::NamedValueSet controlPos;     ///< control id -> [x, y]
    bool anyValid = false;
};

} // namespace bv::ui
