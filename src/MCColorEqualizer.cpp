// =============================================================================
// MCColorEqualizer.cpp
// -----------------------------------------------------------------------------
// 10-Band Color Equalizer OFX plugin.
// Pipeline: Parallel — all EQs read the same input RGB (1 roundtrip).
//
// GPU dispatch:
//   macOS   → Metal
//   Win/Lin → CUDA
//
// Ported from:
//   MC Color Equalizer Hue.dctl + Sat.dctl + Bri.dctl
// =============================================================================

#include "MCColorEqualizer.h"
#include "ColorMath.h"
#include "EQParams.h"
#include "MCOpenNexPlatform.h"
#include "MCOpenNexPresenter.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"

// ─── Plugin identity constants ─────────────────────────────────────────────
#ifndef PLUGIN_VERSION
#define kPluginVersion "v0.0.1"
#else
#define kPluginVersion PLUGIN_VERSION
#endif

#define kPluginName "Color Equalizer"
#define kPluginNameLabel "Color Equalizer " kPluginVersion
#define kPluginGrouping "MC Plugins"
#define kPluginDescription                                                     \
  "10-Band Color Equalizer with Hue, Saturation and Brightness controls. "     \
  "Supports RGB Direct, RGB Spherical and OKLCH color models."
#define kPluginIdentifier "com.MCColorEqualizer"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

// ─── Capability flags ──────────────────────────────────────────────────────
#define kSupportsTiles false
#define kSupportsMultiResolution false
#define kSupportsMultipleClipPARs false

// ─── Parameter names ──────────────────────────────────────────────────────
#define kParamInputCS "inputColorSpace"
#define kParamSpaceType "spaceType"
#define kParamHueMaster "hueMaster"
#define kParamSatMaster "satMaster"
#define kParamLumMaster "lumMaster"
#define kParamAboutHelp "aboutHelp"
#define kParamUpdateStatus "updateStatus"
#define kParamCheckUpdates "checkUpdates"
#define kParamAppMCNexus "appMCNexus"

#define kAboutHelpUrl "https://github.com/ciqueira/ColorEqualizer"

static const char *kHueNames[10] = {
    "hueRed",  "hueOrange", "hueYellow", "hueLime",   "hueGreen",
    "hueTeal", "hueCyan",   "hueBlue",   "huePurple", "hueMagenta"};
static const char *kHueLabels[10] = {
    "Hue Red",  "Hue Orange", "Hue Yellow", "Hue Lime",   "Hue Green",
    "Hue Teal", "Hue Cyan",   "Hue Blue",   "Hue Purple", "Hue Magenta"};
static const char *kSatNames[10] = {
    "satRed",  "satOrange", "satYellow", "satLime",   "satGreen",
    "satTeal", "satCyan",   "satBlue",   "satPurple", "satMagenta"};
static const char *kSatLabels[10] = {
    "Sat Red",  "Sat Orange", "Sat Yellow", "Sat Lime",   "Sat Green",
    "Sat Teal", "Sat Cyan",   "Sat Blue",   "Sat Purple", "Sat Magenta"};
static const char *kLumNames[10] = {
    "lumRed",  "lumOrange", "lumYellow", "lumLime",   "lumGreen",
    "lumTeal", "lumCyan",   "lumBlue",   "lumPurple", "lumMagenta"};
static const char *kLumLabels[10] = {
    "Luma Red",  "Luma Orange", "Luma Yellow", "Luma Lime",   "Luma Green",
    "Luma Teal", "Luma Cyan",   "Luma Blue",   "Luma Purple", "Luma Magenta"};

// ─── Space type UI → internal mapping ──────────────────────────────────────
static int mapSpaceType(int uiIndex) {
  switch (uiIndex) {
  case 0:
    return -1; // RGB Direct
  case 1:
    return 8; // RGB Spherical
  case 2:
    return 11; // OKLCH
  default:
    return 8;
  }
}

static void openMCNexusApp() {
  if (mcopen::openMCNexusApplication()) {
    return;
  }
  if (mcopen::canOpenUrl("mcnexus://home") &&
      mcopen::openUrl("mcnexus://home")) {
    return;
  }
  mcopen::openUrl("https://mcnexus.app");
}

// =============================================================================
// GPU kernel forward declarations
// =============================================================================

#ifndef __APPLE__
extern "C" bool
RunCudaKernel(void *p_Stream, int p_RenderX1, int p_RenderY1, int p_RenderWidth,
              int p_RenderHeight, int p_SrcBoundsX1, int p_SrcBoundsY1,
              int p_DstBoundsX1, int p_DstBoundsY1, int p_SrcRowBytes,
              int p_DstRowBytes, int p_InputPremultiplied,
              int p_OutputPremultiplied, int p_inputCS, int p_spaceType,
              const float *p_Input, float *p_Output, const float *p_Lut);
#endif

#ifdef __APPLE__
extern "C" bool
RunMetalKernel(void *p_CmdQ, int p_RenderX1, int p_RenderY1, int p_RenderWidth,
               int p_RenderHeight, int p_SrcBoundsX1, int p_SrcBoundsY1,
               int p_DstBoundsX1, int p_DstBoundsY1, int p_SrcRowBytes,
               int p_DstRowBytes, int p_InputPremultiplied,
               int p_OutputPremultiplied, int p_inputCS, int p_spaceType,
               const float *p_Input, float *p_Output, const float *p_Lut);
#endif

// =============================================================================
// ColorEqualizerProcessor — OFX ImageProcessor subclass
// =============================================================================

class ColorEqualizerProcessor : public OFX::ImageProcessor {
public:
  explicit ColorEqualizerProcessor(OFX::ImageEffect &p_Instance)
      : OFX::ImageProcessor(p_Instance), _srcImg(nullptr) {
    memset(&_params, 0, sizeof(_params));
    memset(_lut, 0, sizeof(_lut));
  }

  void buildLut() {
    colormath::build_equalizer_lut(
        _lut, 256, _params.spaceType, _params.hueVals, _params.hueMaster,
        _params.satVals, _params.satMaster, _params.lumVals, _params.lumMaster);
  }

  // ── GPU overrides ─────────────────────────────────────────────────────

  virtual void processImagesCuda() override {
#ifndef __APPLE__
    const OfxRectI &srcBounds = _srcImg->getBounds();
    const OfxRectI &dstBounds = _dstImg->getBounds();
    const int renderX1 =
        std::max(_renderWindow.x1, std::max(srcBounds.x1, dstBounds.x1));
    const int renderY1 =
        std::max(_renderWindow.y1, std::max(srcBounds.y1, dstBounds.y1));
    const int renderX2 =
        std::min(_renderWindow.x2, std::min(srcBounds.x2, dstBounds.x2));
    const int renderY2 =
        std::min(_renderWindow.y2, std::min(srcBounds.y2, dstBounds.y2));
    if (renderX1 >= renderX2 || renderY1 >= renderY2)
      return;

    const int srcRowBytes = _srcImg->getRowBytes();
    const int dstRowBytes = _dstImg->getRowBytes();
    if (srcRowBytes <= 0 || dstRowBytes <= 0)
      OFX::throwSuiteStatusException(kOfxStatErrUnsupported);

    const float *input = static_cast<const float *>(_srcImg->getPixelData());
    float *output = static_cast<float *>(_dstImg->getPixelData());
    const bool inputPremultiplied =
        _srcImg->getPreMultiplication() == OFX::eImagePreMultiplied;
    const bool outputPremultiplied =
        _dstImg->getPreMultiplication() == OFX::eImagePreMultiplied;

    const bool succeeded =
        RunCudaKernel(_pCudaStream, renderX1, renderY1, renderX2 - renderX1,
                      renderY2 - renderY1, srcBounds.x1, srcBounds.y1,
                      dstBounds.x1, dstBounds.y1, srcRowBytes, dstRowBytes,
                      inputPremultiplied ? 1 : 0, outputPremultiplied ? 1 : 0,
                      _params.inputCS, _params.spaceType, input, output, _lut);
    if (!succeeded)
      OFX::throwSuiteStatusException(kOfxStatFailed);
#endif
  }

  virtual void processImagesMetal() override {
#ifdef __APPLE__
    const OfxRectI &srcBounds = _srcImg->getBounds();
    const OfxRectI &dstBounds = _dstImg->getBounds();
    const int renderX1 =
        std::max(_renderWindow.x1, std::max(srcBounds.x1, dstBounds.x1));
    const int renderY1 =
        std::max(_renderWindow.y1, std::max(srcBounds.y1, dstBounds.y1));
    const int renderX2 =
        std::min(_renderWindow.x2, std::min(srcBounds.x2, dstBounds.x2));
    const int renderY2 =
        std::min(_renderWindow.y2, std::min(srcBounds.y2, dstBounds.y2));
    if (renderX1 >= renderX2 || renderY1 >= renderY2)
      return;

    const int srcRowBytes = _srcImg->getRowBytes();
    const int dstRowBytes = _dstImg->getRowBytes();
    if (srcRowBytes <= 0 || dstRowBytes <= 0)
      OFX::throwSuiteStatusException(kOfxStatErrUnsupported);

    const float *input = static_cast<const float *>(_srcImg->getPixelData());
    float *output = static_cast<float *>(_dstImg->getPixelData());
    const bool inputPremultiplied =
        _srcImg->getPreMultiplication() == OFX::eImagePreMultiplied;
    const bool outputPremultiplied =
        _dstImg->getPreMultiplication() == OFX::eImagePreMultiplied;

    const bool succeeded =
        RunMetalKernel(_pMetalCmdQ, renderX1, renderY1, renderX2 - renderX1,
                       renderY2 - renderY1, srcBounds.x1, srcBounds.y1,
                       dstBounds.x1, dstBounds.y1, srcRowBytes, dstRowBytes,
                       inputPremultiplied ? 1 : 0, outputPremultiplied ? 1 : 0,
                       _params.inputCS, _params.spaceType, input, output, _lut);
    if (!succeeded)
      OFX::throwSuiteStatusException(kOfxStatFailed);
#endif
  }

  // ── Setters ───────────────────────────────────────────────────────────
  void setSrcImg(OFX::Image *p_Img) { _srcImg = p_Img; }
  void setParams(const EQParams &p) { _params = p; }

private:
  OFX::Image *_srcImg;
  EQParams _params;
  float _lut[256 * 3];
};

// =============================================================================
// MCColorEqualizerPlugin — OFX ImageEffect subclass
// =============================================================================

class MCColorEqualizerPlugin : public OFX::ImageEffect {
public:
  explicit MCColorEqualizerPlugin(OfxImageEffectHandle p_Handle);

  virtual void render(const OFX::RenderArguments &p_Args) override;
  virtual bool isIdentity(const OFX::IsIdentityArguments &p_Args,
                          OFX::Clip *&p_IdentityClip,
                          double &p_IdentityTime) override;
  virtual void changedParam(const OFX::InstanceChangedArgs &p_Args,
                            const std::string &p_ParamName) override;

private:
  void setupAndProcess(ColorEqualizerProcessor &p_Processor,
                       const OFX::RenderArguments &p_Args);
  EQParams getActiveParams(double time);
  void refreshUpdateUi();

  OFX::Clip *m_SrcClip;
  OFX::Clip *m_DstClip;

  // Input settings
  OFX::ChoiceParam *m_InputCS;
  OFX::ChoiceParam *m_SpaceType;

  // Hue
  OFX::DoubleParam *m_HueMaster;
  OFX::DoubleParam *m_Hue[10];

  // Sat
  OFX::DoubleParam *m_SatMaster;
  OFX::DoubleParam *m_Sat[10];

  // Luma
  OFX::DoubleParam *m_LumMaster;
  OFX::DoubleParam *m_Lum[10];

  // MCOpenNex update presenter
  OFX::StringParam *m_UpdateStatus;
  std::unique_ptr<MCOpenNexPresenter> m_UpdatePresenter;
  bool m_UpdatingSupportUi;
};

// ─── Constructor ──────────────────────────────────────────────────────────

MCColorEqualizerPlugin::MCColorEqualizerPlugin(OfxImageEffectHandle p_Handle)
    : OFX::ImageEffect(p_Handle), m_UpdateStatus(nullptr),
      m_UpdatingSupportUi(false) {
  m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
  m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

  m_InputCS = fetchChoiceParam(kParamInputCS);
  m_SpaceType = fetchChoiceParam(kParamSpaceType);

  m_HueMaster = fetchDoubleParam(kParamHueMaster);
  for (int i = 0; i < 10; i++)
    m_Hue[i] = fetchDoubleParam(kHueNames[i]);

  m_SatMaster = fetchDoubleParam(kParamSatMaster);
  for (int i = 0; i < 10; i++)
    m_Sat[i] = fetchDoubleParam(kSatNames[i]);

  m_LumMaster = fetchDoubleParam(kParamLumMaster);
  for (int i = 0; i < 10; i++)
    m_Lum[i] = fetchDoubleParam(kLumNames[i]);

  m_UpdateStatus = fetchStringParam(kParamUpdateStatus);

  const OFX::ImageEffectHostDescription *host =
      OFX::getImageEffectHostDescription();
  std::string hostVersion;
  const char *hostName = "";
  if (host) {
    hostName = host->hostName.c_str();
    if (host->versionMajor >= 0 && host->versionMinor >= 0 &&
        host->versionMicro >= 0) {
      hostVersion = std::to_string(host->versionMajor) + "." +
                    std::to_string(host->versionMinor) + "." +
                    std::to_string(host->versionMicro);
    }
  }
  m_UpdatePresenter.reset(
      new MCOpenNexPresenter(kPluginVersion, hostName, hostVersion.c_str()));
  m_UpdatePresenter->requestCheck(false);
  refreshUpdateUi();
}

// ─── changedParam ─────────────────────────────────────────────────────────

void MCColorEqualizerPlugin::changedParam(const OFX::InstanceChangedArgs &,
                                          const std::string &p_ParamName) {
  if (m_UpdatingSupportUi || p_ParamName == kParamUpdateStatus) {
    return;
  }
  refreshUpdateUi();

  if (p_ParamName == kParamAboutHelp) {
    mcopen::openUrl(kAboutHelpUrl);
  } else if (p_ParamName == kParamCheckUpdates) {
    // Keep the last pulled result visible while the forced check runs.
    m_UpdatePresenter->requestCheck(true);
  } else if (p_ParamName == kParamAppMCNexus) {
    if (!m_UpdatePresenter->openPrimaryAction()) {
      openMCNexusApp();
    }
  }
}

void MCColorEqualizerPlugin::refreshUpdateUi() {
  if (!m_UpdateStatus || !m_UpdatePresenter || m_UpdatingSupportUi) {
    return;
  }
  m_UpdatingSupportUi = true;
  const MCOpenNexPresenter::ViewState state =
      m_UpdatePresenter->viewState();
  try {
    m_UpdateStatus->setValue(state.status);
  } catch (...) {
    m_UpdatingSupportUi = false;
    throw;
  }
  m_UpdatingSupportUi = false;
}

// ─── getActiveParams ──────────────────────────────────────────────────────

EQParams MCColorEqualizerPlugin::getActiveParams(double time) {
  EQParams p;
  int ics = 0;
  m_InputCS->getValueAtTime(time, ics);
  p.inputCS = ics;

  int st = 0;
  m_SpaceType->getValueAtTime(time, st);
  p.spaceType = mapSpaceType(st);

  p.hueMaster = static_cast<float>(m_HueMaster->getValueAtTime(time));
  for (int i = 0; i < 10; i++)
    p.hueVals[i] = static_cast<float>(m_Hue[i]->getValueAtTime(time));

  p.satMaster = static_cast<float>(m_SatMaster->getValueAtTime(time));
  for (int i = 0; i < 10; i++)
    p.satVals[i] = static_cast<float>(m_Sat[i]->getValueAtTime(time));

  p.lumMaster = static_cast<float>(m_LumMaster->getValueAtTime(time));
  for (int i = 0; i < 10; i++)
    p.lumVals[i] = static_cast<float>(m_Lum[i]->getValueAtTime(time));

  return p;
}

// ─── render ───────────────────────────────────────────────────────────────

void MCColorEqualizerPlugin::render(const OFX::RenderArguments &p_Args) {
  if ((m_DstClip->getPixelDepth() == OFX::eBitDepthFloat) &&
      (m_DstClip->getPixelComponents() == OFX::ePixelComponentRGBA)) {
    ColorEqualizerProcessor processor(*this);
    setupAndProcess(processor, p_Args);
  } else {
    OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
  }
}

// ─── isIdentity ───────────────────────────────────────────────────────────

bool MCColorEqualizerPlugin::isIdentity(const OFX::IsIdentityArguments &p_Args,
                                        OFX::Clip *&p_IdentityClip,
                                        double &p_IdentityTime) {
  return false;
}

// ─── setupAndProcess ──────────────────────────────────────────────────────

void MCColorEqualizerPlugin::setupAndProcess(
    ColorEqualizerProcessor &p_Processor, const OFX::RenderArguments &p_Args) {
#ifdef __APPLE__
  if (!p_Args.isEnabledMetalRender)
    OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
#else
  if (!p_Args.isEnabledCudaRender)
    OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
#endif

  std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
  std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));

  if ((src->getPixelDepth() != dst->getPixelDepth()) ||
      (src->getPixelComponents() != dst->getPixelComponents())) {
    OFX::throwSuiteStatusException(kOfxStatErrValue);
  }

  EQParams params = getActiveParams(p_Args.time);

  p_Processor.setDstImg(dst.get());
  p_Processor.setSrcImg(src.get());
  p_Processor.setGPURenderArgs(p_Args);
  p_Processor.setRenderWindow(p_Args.renderWindow);
  p_Processor.setParams(params);
  p_Processor.buildLut();
#ifdef __APPLE__
  p_Processor.processImagesMetal();
#else
  p_Processor.processImagesCuda();
#endif
}

// =============================================================================
// MCColorEqualizerFactory — plugin factory
// =============================================================================

MCColorEqualizerFactory::MCColorEqualizerFactory()
    : OFX::PluginFactoryHelper<MCColorEqualizerFactory>(
          kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor) {}

// ─── describe ─────────────────────────────────────────────────────────────

void MCColorEqualizerFactory::describe(OFX::ImageEffectDescriptor &p_Desc) {
  p_Desc.setLabels(kPluginNameLabel, kPluginNameLabel, kPluginNameLabel);
  p_Desc.setPluginGrouping(kPluginGrouping);
  p_Desc.setPluginDescription(kPluginDescription);

  p_Desc.addSupportedContext(OFX::eContextFilter);
  p_Desc.addSupportedContext(OFX::eContextGeneral);
  p_Desc.addSupportedBitDepth(OFX::eBitDepthFloat);

  p_Desc.setSingleInstance(false);
  p_Desc.setHostFrameThreading(false);
  p_Desc.setSupportsMultiResolution(kSupportsMultiResolution);
  p_Desc.setSupportsTiles(kSupportsTiles);
  p_Desc.setTemporalClipAccess(false);
  p_Desc.setRenderTwiceAlways(false);
  p_Desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);

#ifdef __APPLE__
  p_Desc.setSupportsMetalRender(true);
#else
  p_Desc.setSupportsCudaRender(true);
  p_Desc.setSupportsCudaStream(true);
#endif
}

// ─── describeInContext ────────────────────────────────────────────────────

void MCColorEqualizerFactory::describeInContext(
    OFX::ImageEffectDescriptor &p_Desc, OFX::ContextEnum /*p_Context*/) {
  // ── Clips ─────────────────────────────────────────────────────────────
  OFX::ClipDescriptor *srcClip =
      p_Desc.defineClip(kOfxImageEffectSimpleSourceClipName);
  srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
  srcClip->setTemporalClipAccess(false);
  srcClip->setSupportsTiles(kSupportsTiles);
  srcClip->setIsMask(false);

  OFX::ClipDescriptor *dstClip =
      p_Desc.defineClip(kOfxImageEffectOutputClipName);
  dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
  dstClip->setSupportsTiles(kSupportsTiles);

  // ── Parameters page ───────────────────────────────────────────────────
  OFX::PageParamDescriptor *page = p_Desc.definePageParam("Controls");

  // ════════════════════════════════════════════════════════════════════
  // Input Space & Model (top-level, no group)
  // ════════════════════════════════════════════════════════════════════
  {
    OFX::ChoiceParamDescriptor *ics = p_Desc.defineChoiceParam(kParamInputCS);
    ics->setLabels("Input Space", "Input Space", "Input Space");
    ics->appendOption("ACES AP1 / ACEScct");
    ics->appendOption("DaVinci Wide Gamut / Intermediate");
    ics->appendOption("ARRI Wide Gamut 3 / LogC3");
    ics->appendOption("ARRI Wide Gamut 4 / LogC4");
    ics->setDefault(1); // DWG
    page->addChild(*ics);

    OFX::ChoiceParamDescriptor *spt = p_Desc.defineChoiceParam(kParamSpaceType);
    spt->setLabels("Model / Space Type", "Model / Space Type",
                   "Model / Space Type");
    spt->appendOption("RGB Direct");
    spt->appendOption("RGB Spherical");
    spt->appendOption("OKLCH");
    spt->setDefault(1); // RGB Spherical
    page->addChild(*spt);
  }

  // ════════════════════════════════════════════════════════════════════
  // Group: Hue Equalizer
  // ════════════════════════════════════════════════════════════════════
  {
    OFX::GroupParamDescriptor *grp = p_Desc.defineGroupParam("grpHue");
    grp->setLabels("Hue Equalizer", "Hue Equalizer", "Hue Equalizer");
    grp->setOpen(true);
    page->addChild(*grp);

    OFX::DoubleParamDescriptor *master =
        p_Desc.defineDoubleParam(kParamHueMaster);
    master->setLabels("Hue Master Effect", "Hue Master", "Hue Master Effect");
    master->setHint("Scales the intensity of all hue shifts.");
    master->setDefault(1.0);
    master->setRange(0.0, 2.0);
    master->setDisplayRange(0.0, 2.0);
    master->setIncrement(0.01);
    master->setDoubleType(OFX::eDoubleTypePlain);
    master->setParent(*grp);
    page->addChild(*master);

    for (int i = 0; i < 10; i++) {
      OFX::DoubleParamDescriptor *p = p_Desc.defineDoubleParam(kHueNames[i]);
      p->setLabels(kHueLabels[i], kHueLabels[i], kHueLabels[i]);
      p->setDefault(0.0);
      p->setRange(-1.0, 1.0);
      p->setDisplayRange(-1.0, 1.0);
      p->setIncrement(0.01);
      p->setDoubleType(OFX::eDoubleTypePlain);
      p->setParent(*grp);
      page->addChild(*p);
    }
  }

  // ════════════════════════════════════════════════════════════════════
  // Group: Saturation Equalizer
  // ════════════════════════════════════════════════════════════════════
  {
    OFX::GroupParamDescriptor *grp = p_Desc.defineGroupParam("grpSat");
    grp->setLabels("Saturation Equalizer", "Sat Equalizer",
                   "Saturation Equalizer");
    grp->setOpen(true);
    page->addChild(*grp);

    OFX::DoubleParamDescriptor *master =
        p_Desc.defineDoubleParam(kParamSatMaster);
    master->setLabels("Sat Master Effect", "Sat Master", "Sat Master Effect");
    master->setHint("Scales the intensity of all saturation adjustments.");
    master->setDefault(1.0);
    master->setRange(0.0, 2.0);
    master->setDisplayRange(0.0, 2.0);
    master->setIncrement(0.01);
    master->setDoubleType(OFX::eDoubleTypePlain);
    master->setParent(*grp);
    page->addChild(*master);

    for (int i = 0; i < 10; i++) {
      OFX::DoubleParamDescriptor *p = p_Desc.defineDoubleParam(kSatNames[i]);
      p->setLabels(kSatLabels[i], kSatLabels[i], kSatLabels[i]);
      p->setDefault(1.0);
      p->setRange(0.0, 2.0);
      p->setDisplayRange(0.0, 2.0);
      p->setIncrement(0.01);
      p->setDoubleType(OFX::eDoubleTypePlain);
      p->setParent(*grp);
      page->addChild(*p);
    }
  }

  // ════════════════════════════════════════════════════════════════════
  // Group: Brightness (Luma) Equalizer
  // ════════════════════════════════════════════════════════════════════
  {
    OFX::GroupParamDescriptor *grp = p_Desc.defineGroupParam("grpLum");
    grp->setLabels("Brightness Equalizer", "Bri Equalizer",
                   "Brightness Equalizer");
    grp->setOpen(true);
    page->addChild(*grp);

    OFX::DoubleParamDescriptor *master =
        p_Desc.defineDoubleParam(kParamLumMaster);
    master->setLabels("Luma Master Effect", "Luma Master",
                      "Luma Master Effect");
    master->setHint("Scales the intensity of all brightness adjustments.");
    master->setDefault(1.0);
    master->setRange(0.0, 2.0);
    master->setDisplayRange(0.0, 2.0);
    master->setIncrement(0.01);
    master->setDoubleType(OFX::eDoubleTypePlain);
    master->setParent(*grp);
    page->addChild(*master);

    for (int i = 0; i < 10; i++) {
      OFX::DoubleParamDescriptor *p = p_Desc.defineDoubleParam(kLumNames[i]);
      p->setLabels(kLumLabels[i], kLumLabels[i], kLumLabels[i]);
      p->setDefault(1.0);
      p->setRange(0.0, 2.0);
      p->setDisplayRange(0.0, 2.0);
      p->setIncrement(0.01);
      p->setDoubleType(OFX::eDoubleTypePlain);
      p->setParent(*grp);
      page->addChild(*p);
    }
  }

  // ════════════════════════════════════════════════════════════════════
  // Group: Support
  // ════════════════════════════════════════════════════════════════════
  {
    OFX::GroupParamDescriptor *grp = p_Desc.defineGroupParam("grpSupport");
    grp->setLabels("Support", "Support", "Support");
    grp->setOpen(false);
    page->addChild(*grp);

    OFX::PushButtonParamDescriptor *aboutHelp =
        p_Desc.definePushButtonParam(kParamAboutHelp);
    aboutHelp->setLabels("About and Help", "About and Help", "About and Help");
    aboutHelp->setParent(*grp);
    page->addChild(*aboutHelp);

    OFX::StringParamDescriptor *updateStatus =
        p_Desc.defineStringParam(kParamUpdateStatus);
    updateStatus->setLabels("Updates", "Updates", "Updates");
    updateStatus->setDefault("Update check has not run yet.");
    updateStatus->setStringType(OFX::eStringTypeMultiLine);
    updateStatus->setEnabled(false);
    updateStatus->setIsPersistant(false);
    updateStatus->setEvaluateOnChange(false);
    updateStatus->setParent(*grp);
    page->addChild(*updateStatus);

    OFX::PushButtonParamDescriptor *checkUpdates =
        p_Desc.definePushButtonParam(kParamCheckUpdates);
    checkUpdates->setLabels("Check for Updates", "Check for Updates",
                            "Check for Updates");
    checkUpdates->setParent(*grp);
    page->addChild(*checkUpdates);

    OFX::PushButtonParamDescriptor *appMCNexus =
        p_Desc.definePushButtonParam(kParamAppMCNexus);
    appMCNexus->setLabels("Open MCNexus", "Open MCNexus", "Open MCNexus");
#if !defined(__APPLE__) && !defined(_WIN32)
    appMCNexus->setEnabled(false);
#endif
    appMCNexus->setParent(*grp);
    page->addChild(*appMCNexus);
  }
}

// ─── createInstance ───────────────────────────────────────────────────────

OFX::ImageEffect *
MCColorEqualizerFactory::createInstance(OfxImageEffectHandle p_Handle,
                                        OFX::ContextEnum /*p_Context*/) {
  return new MCColorEqualizerPlugin(p_Handle);
}

// ─── Plugin entry point ───────────────────────────────────────────────────

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray &p_FactoryArray) {
  static MCColorEqualizerFactory plugin;
  p_FactoryArray.push_back(&plugin);
}
