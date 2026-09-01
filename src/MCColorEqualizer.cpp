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
#include "MCLicense.h"
#include "MCNotice.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif
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
#define kParamAppMCNexus "appMCNexus"
#define kParamNoticeText "noticeText"
#define kParamNoticeUpdate "noticeUpdate"
#define kParamNoticeOpenNexus "noticeOpenNexus"
#define kParamLicenseRefreshBtn "licenseRefreshBtn"
#define kParamDiagHost "diagHost"
#define kParamLicenseStatus "licenseStatus"
#define kParamLicenseEdition "licenseEdition"
#define kParamLicenseValidity "licenseValidity"
#define kParamLicenseActivation "licenseActivation"
#define kParamLicensePath "licensePath"
#define kParamLicenseSync "licenseSync"
#define kParamLicenseRefresh "licenseRefresh"

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

static void openExternalUrl(const char *url) {
#ifdef _WIN32
  ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
  std::string command = "open \"";
  command += url;
  command += "\" >/dev/null 2>&1";
  std::system(command.c_str());
#else
  std::string command = "xdg-open \"";
  command += url;
  command += "\" >/dev/null 2>&1 &";
  std::system(command.c_str());
#endif
}

// Same launchers, but reporting whether it worked.
//
// openExternalUrl() above returns void, which is fine for a fixed https link
// that always resolves. It is not fine here: the SDK decides whether to fall
// back from the mcnexus:// deep link to the https:// one by asking whether
// the first attempt opened, so swallowing the failure would leave anyone
// without MCNexus installed pressing a button that does nothing.
static int openUrlForNotice(const char *url) {
  if (!url || url[0] == '\0') return 0;
#ifdef _WIN32
  HINSTANCE result =
      ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
  return reinterpret_cast<intptr_t>(result) > 32 ? 1 : 0;
#elif defined(__APPLE__)
  // `open` exits non-zero when no application claims the scheme, which is
  // exactly the signal needed and costs nothing to read.
  std::string command = "open \"";
  command += url;
  command += "\" >/dev/null 2>&1";
  return std::system(command.c_str()) == 0 ? 1 : 0;
#else
  std::string command = "xdg-open \"";
  command += url;
  command += "\" >/dev/null 2>&1";
  return std::system(command.c_str()) == 0 ? 1 : 0;
#endif
}

static void openMCNexusApp() {
#ifdef __APPLE__
  std::system(
      "open -a MCNexus >/dev/null 2>&1 || open \"/Applications/MCNexus.app\" "
      ">/dev/null 2>&1");
#elif defined(_WIN32)
  auto shellExecuteWindowsPath = [](const wchar_t *path,
                                    const wchar_t *parameters) {
    HINSTANCE result =
        ShellExecuteW(nullptr, L"open", path, parameters, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
  };

  auto launchPowerShellHidden = [](const wchar_t *parameters) {
    std::wstring commandLine = L"powershell.exe ";
    commandLine += parameters;

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION processInfo = {};
    const BOOL created = CreateProcessW(
        nullptr, &commandLine[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startupInfo, &processInfo);
    if (!created) {
      return false;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
  };

  auto launchWindowsExecutableIfExists = [&](const wchar_t *pathWithEnvironment) {
    wchar_t expanded[MAX_PATH] = {};
    const DWORD expandedLength =
        ExpandEnvironmentStringsW(pathWithEnvironment, expanded, MAX_PATH);
    const wchar_t *path =
        (expandedLength > 0 && expandedLength < MAX_PATH) ? expanded
                                                          : pathWithEnvironment;
    const DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
      return false;
    }
    return shellExecuteWindowsPath(path, nullptr);
  };

  if (launchWindowsExecutableIfExists(L"%ProgramFiles%\\MCNexus\\MCNexus.exe") ||
      launchWindowsExecutableIfExists(
          L"%ProgramFiles(x86)%\\MCNexus\\MCNexus.exe") ||
      launchWindowsExecutableIfExists(
          L"%LocalAppData%\\Programs\\MCNexus\\MCNexus.exe")) {
    return;
  }

  constexpr const wchar_t *kPowerShellArgs =
      LR"PS(-NoProfile -WindowStyle Hidden -Command "$app = Get-StartApps | Where-Object { $_.Name -eq 'MCNexus' } | Select-Object -First 1; if ($app) { Start-Process ('shell:AppsFolder\' + $app.AppID) } else { Start-Process 'https://apps.microsoft.com/detail/9n1qqt1xc825?hl=en-US&gl=US' }")PS";
  if (launchPowerShellHidden(kPowerShellArgs)) {
    return;
  }

  openExternalUrl("https://apps.microsoft.com/detail/9n1qqt1xc825?hl=en-US&gl=US");
#else
  openExternalUrl("https://mcnexus.app");
#endif
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
  static EQParams neutralParams();

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

  // Licensing (Perfil A — read only, never activates: D15)
  mc::License m_License;

  // Remote notices and available releases only — never licence state, which
  // gets its own behaviour and is deliberately not folded in here.
  void publishNotice();

  // Hides or restores every control when the licence verdict changes. The
  // effect already refuses to render without a licence; this is the half that
  // stops the panel from offering knobs that do nothing.
  void applyLicenseGate();
  OFX::GroupParam *m_HueGroup = nullptr;
  OFX::GroupParam *m_SatGroup = nullptr;
  OFX::GroupParam *m_LumGroup = nullptr;

  // Guards against publishNotice()'s own writes re-entering changedParam.
  bool m_Publishing = false;
  mc::Notices m_Notices;
  OFX::GroupParam *m_NoticeGroup = nullptr;
  OFX::StringParam *m_NoticeText = nullptr;
  OFX::PushButtonParam *m_NoticeUpdate = nullptr;
  OFX::PushButtonParam *m_NoticeOpenNexus = nullptr;

#ifdef MC_NEXKEY_DIAGNOSTICS
  // Test instrument, compiled out of any shippable build (NEXKEY_DIAGNOSTICS=1).
  void publishLicenseReport();
  OFX::StringParam *m_LicenseStatus = nullptr;
  OFX::StringParam *m_LicenseEdition = nullptr;
  OFX::StringParam *m_LicenseValidity = nullptr;
  OFX::StringParam *m_LicenseActivation = nullptr;
  OFX::StringParam *m_LicenseSync = nullptr;
  OFX::StringParam *m_LicensePath = nullptr;
  OFX::StringParam *m_DiagHost = nullptr;
#endif
};

// ─── Constructor ──────────────────────────────────────────────────────────

MCColorEqualizerPlugin::MCColorEqualizerPlugin(OfxImageEffectHandle p_Handle)
    : OFX::ImageEffect(p_Handle) {
  m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
  m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

  m_NoticeGroup = fetchGroupParam("grpNotice");
  m_NoticeText = fetchStringParam(kParamNoticeText);
  m_NoticeUpdate = fetchPushButtonParam(kParamNoticeUpdate);
  m_NoticeOpenNexus = fetchPushButtonParam(kParamNoticeOpenNexus);

#ifdef MC_NEXKEY_DIAGNOSTICS
  m_LicenseStatus = fetchStringParam(kParamLicenseStatus);
  m_LicenseEdition = fetchStringParam(kParamLicenseEdition);
  m_LicenseValidity = fetchStringParam(kParamLicenseValidity);
  m_LicenseActivation = fetchStringParam(kParamLicenseActivation);
  m_LicenseSync = fetchStringParam(kParamLicenseSync);
  m_LicensePath = fetchStringParam(kParamLicensePath);
  m_DiagHost = fetchStringParam(kParamDiagHost);
#endif

  // Off the render thread, once per instance: the signature verification and
  // the disk reads all happen here so render() only ever does an atomic load.
  m_License.start();

  // Starts the first update check and does not wait for it. Whatever comes
  // back lands on screen at the next InstanceChanged — OFX offers no timer
  // and parameters may only be written from the UI thread.
  //
  // The host identifies itself so notices scoped to a host and version can
  // match. Passed through verbatim: which string Resolve reports is not
  // documented anywhere and the diagnostics panel exists to find out.
  {
    const OFX::ImageEffectHostDescription *host =
        OFX::getImageEffectHostDescription();
    std::string hostVersion;
    if (host) {
      char buffer[48];
      std::snprintf(buffer, sizeof(buffer), "%d.%d.%d", host->versionMajor,
                    host->versionMinor, host->versionMicro);
      hostVersion = buffer;
    }
    m_Notices.start(&openUrlForNotice, host ? host->hostName.c_str() : nullptr,
                    hostVersion.c_str());
  }

  m_InputCS = fetchChoiceParam(kParamInputCS);
  m_SpaceType = fetchChoiceParam(kParamSpaceType);

  m_HueGroup = fetchGroupParam("grpHue");
  m_HueMaster = fetchDoubleParam(kParamHueMaster);
  for (int i = 0; i < 10; i++)
    m_Hue[i] = fetchDoubleParam(kHueNames[i]);

  m_SatGroup = fetchGroupParam("grpSat");
  m_SatMaster = fetchDoubleParam(kParamSatMaster);
  for (int i = 0; i < 10; i++)
    m_Sat[i] = fetchDoubleParam(kSatNames[i]);

  m_LumGroup = fetchGroupParam("grpLum");
  m_LumMaster = fetchDoubleParam(kParamLumMaster);
  for (int i = 0; i < 10; i++)
    m_Lum[i] = fetchDoubleParam(kLumNames[i]);

  // Publishing comes LAST, after every fetch above: applyLicenseGate() writes
  // to all of them, and the earlier version of this constructor published
  // before the controls existed as pointers.
  //
  // Same guard as changedParam: these writes can be delivered back as
  // InstanceChanged while the instance is still being constructed, which is a
  // worse version of the same loop — it re-enters an object that is not
  // finished yet.
  m_Publishing = true;
  applyLicenseGate();
  publishNotice();
#ifdef MC_NEXKEY_DIAGNOSTICS
  publishLicenseReport();
#endif
  m_Publishing = false;
}

// ─── changedParam ─────────────────────────────────────────────────────────

void MCColorEqualizerPlugin::changedParam(const OFX::InstanceChangedArgs &p_Args,
                                          const std::string &p_ParamName) {
  // WRITING A PARAMETER FROM HERE CALLS THIS BACK.
  //
  // publishNotice() writes: setValue on the text, setIsSecret on the group and
  // both buttons. Each of those makes the host emit another InstanceChanged,
  // which lands right back here and publishes again — 975 levels deep before
  // Resolve's stack guard killed it. The old code never hit this because it
  // only republished inside the licence buttons' own branches; publishing on
  // EVERY change is what closed the loop.
  //
  // Two barriers, because either alone has a hole. `reason` is the documented
  // fix — the host tags plugin-initiated changes eChangePluginEdit — but it
  // depends on the host tagging them honestly, and a re-entrancy flag costs
  // one bool and does not.
  if (m_Publishing) return;
  if (p_ParamName == kParamAboutHelp) {
    openExternalUrl(kAboutHelpUrl);
  } else if (p_ParamName == kParamAppMCNexus) {
    openMCNexusApp();
  } else if (p_ParamName == kParamNoticeUpdate ||
             p_ParamName == kParamNoticeOpenNexus) {
    // Both buttons do the same thing to the same item — they differ only in
    // what they promise, which is why there are two of them and not one with
    // a runtime label (OFX hosts are not obliged to refresh labels).
    m_Notices.openAction();
  } else if (p_ParamName == kParamLicenseRefreshBtn) {
    // DISK FIRST, ALWAYS. Activation and deactivation both happen in another
    // process, and both land as a change to the receipt on disk — MCNexus
    // writes one when you activate and deletes it when you deactivate. Only
    // load_local() notices either.
    //
    // The first version of this branched on allowed() before re-reading, and
    // got the deactivation case exactly backwards: with the receipt already
    // gone, the plugin still held an ACTIVE snapshot in memory, so allowed()
    // said yes and the click went down the asynchronous path, which never
    // touches the disk. Nothing happened, however many times it was pressed.
    //
    // Re-reading first also makes the activation case cheaper than it was:
    // the receipt MCNexus just wrote is already there, so the controls come
    // back without waiting for the network at all.
    m_License.refresh();

    // Then confirm against the server in the background. Never blocking, in
    // either state.
    //
    // The unlicensed branch used to wait up to 4s here, and that wait was
    // worse than it looked. request_sync() hands the work to the poller and
    // returns OK the moment one exists — which it does, because the plugin
    // opened while still licensed — so syncNow() sat in its 80 x 50ms loop
    // waiting for a callback that could never arrive: the poller had lost the
    // licence key along with the receipt and gone to sleep. Four seconds of
    // frozen host UI, on every click, for nothing.
    //
    // The wait is not needed any more. The case it existed for — "I just
    // activated in MCNexus, show me" — is answered by the disk read above,
    // because activation writes the receipt before the user can even get back
    // to the host. The rarer case, receipt gone but key still present and the
    // server able to restore it, now resolves on the next parameter change,
    // since every one of them re-reads the receipt.
    m_License.syncNowAsync();
  }
#ifdef MC_NEXKEY_DIAGNOSTICS
  else if (p_ParamName == kParamLicenseSync "Btn") {
    // Blocking on purpose, unlike the shipped button: a tester wants the
    // answer before the panel redraws. Talks to the backend now instead of
    // waiting out syncAfter, which is 24h.
    m_License.syncNow();
  } else if (p_ParamName == kParamLicenseRefresh) {
    // Re-reads the receipt from disk. This is what an operator presses after
    // activating in another process — the plugin has no idea that happened
    // until something tells it to look again.
    m_License.refresh();
  }
#endif

  // Every USER change, not just the ones above. This is the only moment OFX
  // offers to consume an update check that finished on a background thread:
  // there is no idle callback, and parameters may not be written from the
  // worker. Cheap — reading two snapshots and, at most, hiding or showing a
  // group.
  //
  // eChangeUserEdit only. eChangePluginEdit is this function's own writes
  // coming back, and eChangeTime fires on every frame of playback — republishing
  // there would put a network-backed check on the playback path.
  if (p_Args.reason != OFX::eChangeUserEdit) return;

  m_Publishing = true;

  // Re-read the receipt before deciding anything. Deactivation happens in
  // another process and leaves no trace this plugin is told about — without
  // this line the panel only ever converges when somebody presses Refresh,
  // and the person sitting in front of an unlicensed plugin has no reason to
  // press it. Rate limited inside, so dragging a slider does not turn into a
  // file read per movement.
  m_License.refreshIfStale();

  m_Notices.poke();
  applyLicenseGate();
  publishNotice();
#ifdef MC_NEXKEY_DIAGNOSTICS
  publishLicenseReport();
#endif
  m_Publishing = false;
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
  // Tell the host up front that a denied instance does nothing, so it can
  // skip the effect entirely instead of running a GPU pass that writes the
  // input back out. setupAndProcess() keeps its own guard regardless: this
  // is an optimisation, and a host that ignores isIdentity must still not
  // get the effect applied.
  if (!m_License.allowed()) {
    p_IdentityClip = m_SrcClip;
    p_IdentityTime = p_Args.time;
    return true;
  }
  return false;
}

// The values every control sits at when it is doing nothing. Not a zeroed
// struct: this effect's neutral is 1.0 on every master and on sat/lum, and
// 0.0 only on hue offsets (EQParams.h).
// Copies the SDK's view of the world into the two display parameters. Called
// after construction and from the Refresh button — never from render.
// The whole of what this plugin says about licensing and releases.
//
// Hiding is the default state, not an edge case: on a healthy, licensed,
// up-to-date install this group never appears at all, and the panel looks
// like a colour tool rather than a licence manager.
// Every control disappears when there is no licence, and comes back when one
// turns up. The image keeps flowing — isIdentity already hands the source
// through untouched — so what the user sees is their footage, unprocessed,
// with nothing on the panel but Support.
//
// Hiding one by one rather than hiding the three groups: OFX does not require
// a host to hide a group's children along with it, and Input Space and Space
// Type do not live in a group at all. The groups are hidden too, so an empty
// header does not sit there.
//
// The values are untouched. A hidden parameter keeps whatever the colourist
// dialled in, so restoring the licence restores their grade rather than a
// default one.
//
// Reads allowed(), not licenseAllows(): in shadow mode allowed() is
// constant-true, so a development build shows everything and renders
// everything, exactly as before. UI and render must agree — a panel that
// empties itself while the effect still applies would be worse than either.
void MCColorEqualizerPlugin::applyLicenseGate() {
  const bool hidden = !m_License.allowed();

  if (m_InputCS) m_InputCS->setIsSecret(hidden);
  if (m_SpaceType) m_SpaceType->setIsSecret(hidden);

  if (m_HueGroup) m_HueGroup->setIsSecret(hidden);
  if (m_HueMaster) m_HueMaster->setIsSecret(hidden);
  if (m_SatGroup) m_SatGroup->setIsSecret(hidden);
  if (m_SatMaster) m_SatMaster->setIsSecret(hidden);
  if (m_LumGroup) m_LumGroup->setIsSecret(hidden);
  if (m_LumMaster) m_LumMaster->setIsSecret(hidden);

  for (int band = 0; band < 10; ++band) {
    if (m_Hue[band]) m_Hue[band]->setIsSecret(hidden);
    if (m_Sat[band]) m_Sat[band]->setIsSecret(hidden);
    if (m_Lum[band]) m_Lum[band]->setIsSecret(hidden);
  }
}

void MCColorEqualizerPlugin::publishNotice() {
  // Silent without a licence: Support is the only thing on screen then, and a
  // "new version available" banner is not what somebody who cannot render is
  // trying to solve.
  const mc::NoticeView view =
      m_License.allowed() ? m_Notices.view() : mc::NoticeView();

  if (m_NoticeText) m_NoticeText->setValue(view.text);

  // The group AND both buttons. Hiding only the group leaves hosts that
  // render children independently showing a stray "Update" button with no
  // context around it.
  if (m_NoticeGroup) m_NoticeGroup->setIsSecret(!view.visible);
  if (m_NoticeUpdate) m_NoticeUpdate->setIsSecret(!view.offerUpdate);
  if (m_NoticeOpenNexus) m_NoticeOpenNexus->setIsSecret(!view.offerOpenNexus);
}

#ifdef MC_NEXKEY_DIAGNOSTICS
void MCColorEqualizerPlugin::publishLicenseReport() {
  const mc::LicenseReport report = m_License.report();

  if (m_DiagHost) {
    const OFX::ImageEffectHostDescription *host =
        OFX::getImageEffectHostDescription();
    if (!host) {
      m_DiagHost->setValue("(host did not describe itself)");
    } else {
      char buffer[192];
      std::snprintf(buffer, sizeof(buffer), "%s  %d.%d.%d  (\"%s\")",
                    host->hostName.c_str(), host->versionMajor,
                    host->versionMinor, host->versionMicro,
                    host->versionLabel.c_str());
      m_DiagHost->setValue(buffer);
    }
  }

  if (m_LicenseStatus) {
    std::string text = report.status;
    text += report.allowed ? "  (allow)" : "  (deny)";
    // Say it plainly when the verdict is not being acted on. Otherwise a
    // tester reads "deny" while the effect keeps rendering and reasonably
    // concludes the guard is broken.
    if (!report.enforcing) {
      text += report.allowed ? "  — shadow mode" : "  — shadow mode, effect STILL APPLIED";
    }
    m_LicenseStatus->setValue(text);
  }
  if (m_LicenseEdition) m_LicenseEdition->setValue(report.edition.empty() ? "-" : report.edition);
  if (m_LicenseValidity) m_LicenseValidity->setValue(report.validity.empty() ? "-" : report.validity);
  if (m_LicenseActivation) m_LicenseActivation->setValue(report.activation.empty() ? "-" : report.activation);
  if (m_LicenseSync) m_LicenseSync->setValue(report.sync.empty() ? "-" : report.sync);

  if (m_LicensePath) {
    std::string text = report.receiptsDir;
    if (text.empty()) {
      text = "(unresolved)";
    } else if (report.receiptsFound < 0) {
      text += "  [unreadable]";
    } else {
      char suffix[32];
      std::snprintf(suffix, sizeof(suffix), "  [%d receipt(s)]", report.receiptsFound);
      text += suffix;
    }
    m_LicensePath->setValue(text);
  }
}
#endif // MC_NEXKEY_DIAGNOSTICS

EQParams MCColorEqualizerPlugin::neutralParams() {
  EQParams params;
  std::memset(&params, 0, sizeof(params));
  params.inputCS = 0;
  params.spaceType = -1;
  params.hueMaster = 1.0f;
  params.satMaster = 1.0f;
  params.lumMaster = 1.0f;
  for (int band = 0; band < 10; ++band) {
    params.hueVals[band] = 0.0f;
    params.satVals[band] = 1.0f;
    params.lumVals[band] = 1.0f;
  }
  return params;
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

  // The render guard (§7.3). One atomic load inside the SDK — no I/O, no
  // lock, no verification work — because this runs per frame and per tile.
  // Everything expensive already happened when the instance was constructed.
  //
  // Policy on DENY is passthrough, expressed as NEUTRAL PARAMETERS rather
  // than as a separate code path: the same GPU kernel runs and writes the
  // input straight through. Doing it this way means the denied path is
  // exercised by every render, not by a branch that only runs for
  // unlicensed users and therefore never gets tested.
  //
  // Deliberately NOT an error. Throwing here would abort the host's render
  // and could cost a colourist their work over a licensing state they might
  // fix in thirty seconds; a colour tool that quietly does nothing is the
  // least destructive failure available.
  //
  // Fase 7 replaces this with a visible watermark, which needs a `denied`
  // flag threaded into RunMetalKernel/RunCudaKernel and a stripe composited
  // there. That is GPU work this build has not done; until then the license
  // state is read off the plugin's own parameters, which is unambiguous in
  // a way a silent passthrough is not.
  EQParams params = m_License.allowed() ? getActiveParams(p_Args.time)
                                        : neutralParams();

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
  // Group: Notice — FIRST, and invisible almost always
  // ════════════════════════════════════════════════════════════════════
  //
  // Declared here because a parameter cannot be created later: OFX fixes the
  // set at describe() time, so "appears when there is news" has to mean
  // "declared always, revealed by setIsSecret(false) on the instance".
  //
  // First on the page on purpose. It is the only thing in this panel the user
  // may need to act on, and it is absent the rest of the time — so it costs
  // nothing to put it where it will be seen when it does show up.
  {
    OFX::GroupParamDescriptor *grp = p_Desc.defineGroupParam("grpNotice");
    grp->setLabels("Notice", "Notice", "Notice");
    grp->setOpen(true);   // if it is showing at all, it has something to say
    grp->setIsSecret(true);
    page->addChild(*grp);

    OFX::StringParamDescriptor *text =
        p_Desc.defineStringParam(kParamNoticeText);
    text->setLabels("", "", "");
    text->setStringType(OFX::eStringTypeMultiLine);
    text->setEnabled(false);         // display only
    text->setIsPersistant(false);    // recomputed on load; never saved in the project
    text->setParent(*grp);
    page->addChild(*text);

    // Two buttons, one action. A single button with a label rewritten at
    // runtime would be smaller, but OFX does not require a host to refresh a
    // parameter's label after describe() — and "Update" sitting under "your
    // license expired" is worse than an extra param descriptor.
    OFX::PushButtonParamDescriptor *update =
        p_Desc.definePushButtonParam(kParamNoticeUpdate);
    update->setLabels("Update", "Update", "Update");
    update->setIsSecret(true);
    update->setParent(*grp);
    page->addChild(*update);

    OFX::PushButtonParamDescriptor *openNexus =
        p_Desc.definePushButtonParam(kParamNoticeOpenNexus);
    openNexus->setLabels("Open MCNexus", "Open MCNexus", "Open MCNexus");
    openNexus->setIsSecret(true);
    openNexus->setParent(*grp);
    page->addChild(*openNexus);
  }

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
    // Open, and open ALWAYS — not only when unlicensed, because a group
    // cannot be opened at runtime. On the instance, GroupParam exposes
    // getIsOpen() and nothing else; setOpen lives on the descriptor, and the
    // OFX docs call it the group's "initial state". So an unlicensed plugin
    // whose only remaining panel sat collapsed would need a click to reveal
    // the one button that fixes it. Three buttons showing the rest of the
    // time is the cheaper half of that trade.
    grp->setOpen(true);
    page->addChild(*grp);

    OFX::PushButtonParamDescriptor *aboutHelp =
        p_Desc.definePushButtonParam(kParamAboutHelp);
    aboutHelp->setLabels("About and Help", "About and Help", "About and Help");
    aboutHelp->setParent(*grp);
    page->addChild(*aboutHelp);

    // Silent, asynchronous, and click-guarded. It gives no feedback by
    // design: there is no licence panel here to refresh, and the user checks
    // state in MCNexus. What it is FOR is the gap between activating there
    // and this plugin noticing — without it the plugin waits out syncAfter,
    // which is 24h.
    OFX::PushButtonParamDescriptor *refresh =
        p_Desc.definePushButtonParam(kParamLicenseRefreshBtn);
    refresh->setLabels("Refresh License", "Refresh License", "Refresh License");
    refresh->setParent(*grp);
    page->addChild(*refresh);

    OFX::PushButtonParamDescriptor *appMCNexus =
        p_Desc.definePushButtonParam(kParamAppMCNexus);
    appMCNexus->setLabels("App MCNexus", "App MCNexus", "App MCNexus");
#if !defined(__APPLE__) && !defined(_WIN32)
    appMCNexus->setEnabled(false);
#endif
    appMCNexus->setParent(*grp);
    page->addChild(*appMCNexus);
  }

  // ════════════════════════════════════════════════════════════════════
  // Group: License — DIAGNOSTICS ONLY (build with NEXKEY_DIAGNOSTICS=1)
  // ════════════════════════════════════════════════════════════════════
  //
  // Absent from any shippable build. MCNexus already shows edition, seats,
  // validity and activation properly, and a second copy inside every plugin
  // would be a second place to keep correct — the shipped panel says at most
  // one thing, in the Notice group above.
  //
  // It stays available because the Fase 7 matrix still needs it:
  // these two read-only fields are the instrument P11 is run with. The
  // question that item asks is whether a receipt written OUTSIDE the host is
  // found by this plugin INSIDE it — and a sandboxed DaVinci Resolve silently
  // redirects ~/Library/Application Support into its own container, so the
  // answer shows up as a different path here than the one nexkeyctl printed
  // (§3.12).
  //
  // Without them a denied plugin just looks inert, which is indistinguishable
  // from "not activated", "wrong entitlement", "bad ProductData" and
  // "sandbox redirected storage" — the one thing the test needs to tell apart.
#ifdef MC_NEXKEY_DIAGNOSTICS
  {
    OFX::GroupParamDescriptor *grp = p_Desc.defineGroupParam("grpLicense");
    grp->setLabels("License", "License", "License");
    grp->setOpen(false);
    page->addChild(*grp);

    OFX::StringParamDescriptor *status =
        p_Desc.defineStringParam(kParamLicenseStatus);
    status->setLabels("Status", "Status", "Status");
    status->setStringType(OFX::eStringTypeSingleLine);
    status->setEnabled(false);       // display only
    status->setIsPersistant(false);  // recomputed on load; never saved in the project
    status->setParent(*grp);
    page->addChild(*status);

    OFX::StringParamDescriptor *edition =
        p_Desc.defineStringParam(kParamLicenseEdition);
    edition->setLabels("Edition", "Edition", "Edition");
    edition->setStringType(OFX::eStringTypeSingleLine);
    edition->setEnabled(false);
    edition->setIsPersistant(false);
    edition->setParent(*grp);
    page->addChild(*edition);

    OFX::StringParamDescriptor *validity =
        p_Desc.defineStringParam(kParamLicenseValidity);
    validity->setLabels("Validity", "Validity", "Validity");
    validity->setStringType(OFX::eStringTypeSingleLine);
    validity->setEnabled(false);
    validity->setIsPersistant(false);
    validity->setParent(*grp);
    page->addChild(*validity);

    OFX::StringParamDescriptor *activation =
        p_Desc.defineStringParam(kParamLicenseActivation);
    activation->setLabels("Activation", "Activation", "Activation");
    activation->setStringType(OFX::eStringTypeSingleLine);
    activation->setEnabled(false);
    activation->setIsPersistant(false);
    activation->setParent(*grp);
    page->addChild(*activation);

    OFX::StringParamDescriptor *sync =
        p_Desc.defineStringParam(kParamLicenseSync);
    sync->setLabels("Sync", "Sync", "Sync");
    sync->setStringType(OFX::eStringTypeSingleLine);
    sync->setEnabled(false);
    sync->setIsPersistant(false);
    sync->setParent(*grp);
    page->addChild(*sync);

    // What the host calls itself, verbatim. Every manifest written so far
    // assumes "DaVinciResolve" because the spec's example said so; nobody has
    // seen a host say it. A notice scoped to the wrong string matches nobody
    // and fails completely silently, so this is the field that settles it.
    OFX::StringParamDescriptor *hostInfo =
        p_Desc.defineStringParam(kParamDiagHost);
    hostInfo->setLabels("Host", "Host", "Host");
    hostInfo->setStringType(OFX::eStringTypeSingleLine);
    hostInfo->setEnabled(false);
    hostInfo->setIsPersistant(false);
    hostInfo->setParent(*grp);
    page->addChild(*hostInfo);

    OFX::StringParamDescriptor *path =
        p_Desc.defineStringParam(kParamLicensePath);
    path->setLabels("Receipts", "Receipts", "Receipts");
    path->setStringType(OFX::eStringTypeSingleLine);
    path->setEnabled(false);
    path->setIsPersistant(false);
    path->setParent(*grp);
    page->addChild(*path);

    OFX::PushButtonParamDescriptor *syncNow =
        p_Desc.definePushButtonParam(kParamLicenseSync "Btn");
    syncNow->setLabels("Sync Now", "Sync Now", "Sync Now");
    syncNow->setParent(*grp);
    page->addChild(*syncNow);

    OFX::PushButtonParamDescriptor *refresh =
        p_Desc.definePushButtonParam(kParamLicenseRefresh);
    refresh->setLabels("Reload Receipt", "Reload Receipt", "Reload Receipt");
    refresh->setParent(*grp);
    page->addChild(*refresh);
  }
#endif // MC_NEXKEY_DIAGNOSTICS
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
