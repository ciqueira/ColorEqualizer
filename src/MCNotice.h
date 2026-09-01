// =============================================================================
// MCNotice.h
// -----------------------------------------------------------------------------
// The OTHER NexKeyRuntime handle: updates and notices.
//
// It shares nothing with MCLicense.h — no ProductData, no receipt, no
// certificate. Manifests are public and unsigned by design
// (SPEC_UPDATES_NOTICES.md): "0.1.2 exists" is not a claim that needs a
// signature, and keeping the two handles apart is what lets a plugin ship
// updates without licensing, or the reverse.
//
// What it feeds is one collapsed group at the top of the panel that stays
// INVISIBLE until there is something to say. The plugin deliberately carries
// no licence panel: MCNexus already shows edition, seats, validity and
// activation in full, and duplicating that here would be a second place to
// keep correct. The most this plugin ever says is a single notice.
//
// It carries REMOTE notices only — updates and whatever the gateway has to
// say. Licence state deliberately does not appear here: an unlicensed plugin
// gets its own behaviour, designed separately, and folding the two into one
// slot would mean the same widget sometimes reports the server's news and
// sometimes the local verdict.
// =============================================================================

#ifndef MC_NOTICE_H
#define MC_NOTICE_H

#include "MCLicense.h"

#include <string>

namespace mc {

// What the top group renders. Composed from two independent sources — the
// local licence verdict and whatever the gateway had to say — so the panel
// itself needs no rules of its own.
struct NoticeView {
  bool visible = false;
  std::string text;

  // Which of the two buttons the group offers. They are separate params
  // rather than one with a runtime label because OFX hosts are not required
  // to refresh a parameter's label after describe(), and the spec says so in
  // as many words. Hiding and showing is decided by the host, and works.
  bool offerUpdate = false;      // "Update" — there is a release to install
  bool offerOpenNexus = false;   // "Open MCNexus" — go sort the licence out
};

#ifdef MC_NEXKEY_ENABLED

// Where the gateway lives. Baked in from the SAME ProductData the licensing
// handle uses (the Makefile already decodes the blob to print it), so there
// is exactly one place where the environment is stated. The SDK's own default
// is https://sdk.mcnexus.app, which does not resolve today — inheriting it
// silently would turn every check into a network error.
#ifndef MC_NEXKEY_BASE_URL
#define MC_NEXKEY_BASE_URL ""
#endif

#ifndef MC_NEXKEY_ARTIFACT
#define MC_NEXKEY_ARTIFACT "default"
#endif

#ifndef MC_NEXKEY_CHANNEL
#define MC_NEXKEY_CHANNEL "stable"
#endif

// WHICH CHANNEL THIS PLUGIN LISTENS TO.
//
// The SDK exposes two independent channels (SPEC_UPDATES_NOTICES.md): the
// release channel — "there is a newer version" — and the notice channel,
// where the manifest author publishes arbitrary compatibility, security,
// maintenance, migration and deprecation messages.
//
// This plugin listens to the RELEASE channel only, by decision: an OFX plugin
// has one thing worth interrupting a colourist for, and that is a new
// version. The other notice types exist for other kinds of software.
//
// Off rather than deleted: the notice path is written and tested, so turning
// it back on is one flag and no archaeology. With it off, chooseActiveItem is
// told there is no notice, which means the priority rule stays exercised by
// its own tests and does not become dead code that silently rots.
#ifndef MC_NEXKEY_NOTICE_CHANNEL
#define MC_NEXKEY_NOTICE_CHANNEL 0
#endif

inline const char *noticePlatform() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "";
#endif
}

inline const char *noticeArchitecture() {
#if defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#else
  return "x86_64";
#endif
}

enum class ActiveItem { None, Notice, Update };

// The spec's priority, applied here because THE SDK DOES NOT APPLY IT.
//
// SPEC_UPDATES_NOTICES.md describes one ranking over four things —
// critical notice, recommended notice, update, info notice — and says the
// release is folded in as a synthetic notice of type "update". The runtime
// does not do that: evaluator.cpp computes `has_update` on its own and never
// puts it in the notice list, which is sorted by severity alone. So the two
// arrive as independent channels and the ranking has to happen here.
//
// The consequence that actually bites is the info case. Giving the notice
// blanket precedence — the obvious reading, and what this code did at first —
// lets a purely informational notice bury a release the user would want. Only
// `recommended` and `critical` outrank an update.
//
// Pure and free-standing so it can be tested without a handle, a network or a
// host, which is the only reason the table of cases below is checkable at all.
inline ActiveItem chooseActiveItem(bool haveNotice,
                                   NexKeyRuntimeNoticeSeverity severity,
                                   bool hasUpdate) {
  if (haveNotice && (severity > NEXKEYRUNTIME_NOTICE_INFO || !hasUpdate)) {
    return ActiveItem::Notice;
  }
  if (hasUpdate) return ActiveItem::Update;
  return haveNotice ? ActiveItem::Notice : ActiveItem::None;
}

// Owns the update handle for the lifetime of one plugin instance.
// Non-copyable for the same reason License is: destroy() joins the SDK's
// worker, and doing that twice inside a host is a crash, not a leak.
class Notices {
public:
  // How the plugin opens a URL. Passed in rather than called directly so this
  // header stays free of the platform launchers in MCColorEqualizer.cpp.
  // Returns non-zero when the URL actually opened — the SDK uses that answer
  // to fall back from the mcnexus:// deep link to the https:// one, which is
  // what keeps the button useful on a machine with no MCNexus installed.
  using OpenUrlFn = int (*)(const char *url);

  Notices() = default;
  ~Notices() { shutdown(); }
  Notices(const Notices &) = delete;
  Notices &operator=(const Notices &) = delete;

  // hostName/hostVersion come from the OFX host description and are passed
  // straight through, uninterpreted. They are what makes a notice's hostRules
  // decidable — the staging fixture's only notice is scoped to DaVinci
  // Resolve 20.x, and without these it could never match anybody.
  //
  // NOBODY HAS CHECKED what string Resolve actually reports here. Every
  // manifest written so far says "DaVinciResolve" because that is what the
  // spec's example said, not because a host was observed saying it. The
  // diagnostics panel prints the real value for exactly this reason: the
  // first load inside Resolve settles it, and a manifest written against a
  // guess would silently match nothing.
  void start(OpenUrlFn opener, const char *hostName, const char *hostVersion) {
    if (handle_) return;
    opener_ = opener;

    const char *baseUrl = MC_NEXKEY_BASE_URL;
    if (!baseUrl || baseUrl[0] == '\0') return;  // nothing to talk to

    NexKeyRuntimeConfig config;
    nexkeyruntime_config_init(&config);
    config.tenant_id = MC_NEXKEY_TENANT;
    config.artifact_id = MC_NEXKEY_ARTIFACT;
    config.channel = MC_NEXKEY_CHANNEL;
    config.current_version = PLUGIN_VERSION;
    config.platform = noticePlatform();
    config.architecture = noticeArchitecture();
    config.base_url = baseUrl;
    // Left null when the host says nothing. copyConfig() treats absent as
    // "no host declared" and rejects a version string it cannot parse, so
    // handing it an empty or malformed one would kill the whole handle over
    // a field that is only ever an audience filter.
    if (hostName && hostName[0] != '\0') config.host_name = hostName;
    if (hostVersion && hostVersion[0] != '\0') config.host_version = hostVersion;
    config.can_open_url = &Notices::canOpenUrl;
    config.open_url = &Notices::openUrl;
    config.user_data = this;

    handle_ = nexkeyruntime_create(&config);
    if (!handle_) return;

    // Fire the first check immediately and never wait for it. OFX has no idle
    // callback and parameters may only be written from the UI thread, so the
    // result is picked up on the next safe main-thread moment — an
    // InstanceChanged, a slider move — exactly as the spec's "Limitação da
    // API OFX" prescribes. First open on a cold install therefore tends to
    // show nothing, which is the honest outcome and not a bug to paper over.
    nexkeyruntime_request_check(handle_, 0);
  }

  void shutdown() {
    if (!handle_) return;
    // Cancels an in-flight fetch and joins the worker before returning, so
    // the host may unload this bundle the moment it does.
    nexkeyruntime_destroy(handle_);
    handle_ = nullptr;
  }

  // Nudges a fresh check. Cheap and non-blocking: the SDK refuses with BUSY
  // if one is already running, and honours its own schedule unless forced.
  void poke() {
    if (handle_) nexkeyruntime_request_check(handle_, 0);
  }

  // Opens whatever the visible item points at.
  //
  // Uses the SAME decision as view(), through chooseActiveItem, and that is
  // the whole point of routing both through one function: the earlier version
  // opened the notice whenever one existed, so a panel showing "version 0.1.2
  // is available" could open an unrelated maintenance notice instead. Two
  // copies of a priority rule drift; one cannot.
  void openAction() {
    if (!handle_) return;

    NexKeyRuntimeNotice notice{};
    NexKeyRuntimeUpdateSnapshot snapshot{};
    switch (resolve(notice, snapshot)) {
      case ActiveItem::Notice:
        nexkeyruntime_open_notice_action(handle_, notice.id);
        return;
      case ActiveItem::Update:
        nexkeyruntime_open_update_action(handle_);
        return;
      case ActiveItem::None:
        return;
    }
  }

  // Decides what the one visible slot says.
  NoticeView view() const {
    NoticeView out;
    if (!handle_) return out;

    NexKeyRuntimeNotice notice{};
    NexKeyRuntimeUpdateSnapshot snapshot{};
    switch (resolve(notice, snapshot)) {
      case ActiveItem::Notice:
        out.visible = true;
        out.text = notice.title[0] != '\0' ? std::string(notice.title) : std::string();
        if (notice.message[0] != '\0') {
          if (!out.text.empty()) out.text += "\n\n";
          out.text += notice.message;
        }
        // An update notice is the one case where "Update" is the honest verb.
        out.offerUpdate = notice.type == NEXKEYRUNTIME_NOTICE_UPDATE;
        out.offerOpenNexus = !out.offerUpdate;
        return out;

      case ActiveItem::Update:
        out.visible = true;
        out.text = "Version ";
        out.text += snapshot.latest_version;
        out.text += " is available.\n\nYou are running ";
        out.text += snapshot.current_version[0] != '\0' ? snapshot.current_version
                                                        : PLUGIN_VERSION;
        out.text += ".";
        out.offerUpdate = true;
        return out;

      case ActiveItem::None:
        // Silence is the answer for up-to-date, checking, offline and error.
        // An update check that could not reach the network is not news the
        // user can act on, and saying so would put a permanent complaint in
        // the panel of anyone who works offline on purpose.
        return out;
    }
    return out;
  }

private:
  // Reads both sources and applies chooseActiveItem. Fills whichever of the
  // two out-parameters the answer refers to; the other is left zeroed, and
  // ActiveItem says which one to trust.
  ActiveItem resolve(NexKeyRuntimeNotice &notice,
                     NexKeyRuntimeUpdateSnapshot &snapshot) const {
    notice = NexKeyRuntimeNotice();
    notice.struct_size = sizeof(notice);
#if MC_NEXKEY_NOTICE_CHANNEL
    const bool haveNotice =
        nexkeyruntime_get_active_notice(handle_, &notice) == NEXKEYRUNTIME_OK &&
        notice.available;
#else
    // Not even asked for. The gateway may well be serving notices for this
    // tenant — the staging manifest does — and they are ignored here rather
    // than fetched and discarded, so nothing can leak into the panel by way
    // of a later edit further down.
    const bool haveNotice = false;
#endif

    snapshot = NexKeyRuntimeUpdateSnapshot();
    snapshot.struct_size = sizeof(snapshot);
    const bool hasUpdate =
        nexkeyruntime_get_snapshot(handle_, &snapshot) == NEXKEYRUNTIME_OK &&
        snapshot.has_update && snapshot.latest_version[0] != '\0';

    return chooseActiveItem(haveNotice, notice.severity, hasUpdate);
  }

  static int canOpenUrl(void *userData, const char *url) {
    // Answering "yes, try it" for the custom scheme and letting openUrl
    // report the truth. Genuinely probing whether mcnexus:// is registered
    // needs LaunchServices on macOS and a registry walk on Windows, and both
    // would be a worse source of truth than simply attempting it: the SDK
    // falls back to the https:// URL the moment the attempt fails.
    (void)userData;
    return (url && url[0] != '\0') ? 1 : 0;
  }

  static int openUrl(void *userData, const char *url) {
    Notices *self = static_cast<Notices *>(userData);
    if (!self || !self->opener_ || !url || url[0] == '\0') return 0;
    return self->opener_(url);
  }

  NexKeyRuntimeHandle *handle_ = nullptr;
  OpenUrlFn opener_ = nullptr;
};

#else // MC_NEXKEY_ENABLED

// Licensing and updates compiled out. The group never becomes visible, so the
// panel looks exactly as it did before any of this existed.
class Notices {
public:
  using OpenUrlFn = int (*)(const char *url);
  void start(OpenUrlFn, const char *, const char *) {}
  void shutdown() {}
  void poke() {}
  void openAction() {}
  NoticeView view() const { return NoticeView(); }
};

#endif // MC_NEXKEY_ENABLED

} // namespace mc

#endif // MC_NOTICE_H
