// =============================================================================
// MCColorEqualizer.h
// -----------------------------------------------------------------------------
// 10-Band Color Equalizer — Hue, Saturation, Brightness.
// GPU: Metal (macOS), CUDA (Windows/Linux).
// =============================================================================

#pragma once

#include "ofxsImageEffect.h"

class MCColorEqualizerFactory
    : public OFX::PluginFactoryHelper<MCColorEqualizerFactory>
{
public:
    MCColorEqualizerFactory();
    virtual void load()   {}
    virtual void unload() {}
    virtual void describe(OFX::ImageEffectDescriptor& p_Desc);
    virtual void describeInContext(OFX::ImageEffectDescriptor& p_Desc,
                                   OFX::ContextEnum p_Context);
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle p_Handle,
                                             OFX::ContextEnum p_Context);
};
