// =============================================================================
// MCLicense.h
// -----------------------------------------------------------------------------
// NexKeyRuntime glue for MCColorEqualizer — Perfil A only.
//
// This plugin NEVER activates a license (D15). Activation happens in another
// process: MCNexus for end users, `nexkeyctl` while validating. All this does
// is read the receipt that process left on disk and turn it into an
// allow/deny the render path can consult for free.
//
// The whole file compiles to nothing when MC_NEXKEY_ENABLED is undefined, so
// an unlicensed build stays buildable while the integration is being
// validated (P11 / Fase 7).
// =============================================================================

#ifndef MC_LICENSE_H
#define MC_LICENSE_H

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#ifdef MC_NEXKEY_ENABLED
#include <nexkeyruntime/nexkeyruntime.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif
#endif

namespace mc {

// What the UI shows and what P11 compares. Kept as plain strings because its
// only consumers are a read-only parameter and a human reading it off screen.
struct LicenseReport {
  std::string status = "disabled";
  std::string edition;      // the label the certificate carries: demo/trial/full/...
  std::string validity;     // expiry and offline window, in human terms
  std::string activation;   // activation id and seat usage
  std::string receiptsDir;
  std::string sync;         // poller activity: how many, when, and what it said
  int receiptsFound = -1;

  // What the LICENSE says. In shadow mode the effect renders regardless, so
  // this is a report, not necessarily what happened — `enforcing` says which.
  bool allowed = false;
  bool enforcing = false;
};

#ifdef MC_NEXKEY_ENABLED

// Shadow mode (Fase 7: "shadow mode e depois enforcement por coorte").
//
// OFF by default while the integration is being validated: the plugin reads
// the license, reports it in full, and renders the effect either way. That is
// what lets a whole matrix be exercised — demo to trial to full, suspend,
// revoke, seat limits — without the image disappearing every time the answer
// is "deny" and without the tester having to guess whether a black frame
// meant a licensing verdict or a bug.
//
// Build with -DMC_NEXKEY_ENFORCE=1 to make the render guard bite.
#ifndef MC_NEXKEY_ENFORCE
#define MC_NEXKEY_ENFORCE 0
#endif

// The tenant and entitlement this build is licensed under. Both are
// compile-time for a reason: a plugin that could be pointed at another
// tenant at runtime would be trivially convertible into a plugin licensed by
// somebody else's key.
#ifndef MC_NEXKEY_TENANT
#define MC_NEXKEY_TENANT "colorequalizer-oss"
#endif
#ifndef MC_NEXKEY_VARIANT
#define MC_NEXKEY_VARIANT "download:default"
#endif

// What this program calls itself when it talks to the gateway. NOT the same
// thing as the tenant or the entitlement above: those say which licence is
// being checked, this says who is doing the checking.
//
// It matters because an activation is shared. MCNexus activates the licence
// and this plugin validates and syncs against the same record, so without a
// name attached each one's version overwrites the other's and the server
// cannot say which plugin build is deployed. Purely reported — nothing in the
// licensing decision reads it.
#ifndef MC_NEXKEY_PRODUCT
#define MC_NEXKEY_PRODUCT "colorequalizer"
#endif

// The signed ProductData blob, normally baked in at build time
// (-DMC_NEXKEY_PRODUCT_DATA="..."). While validating, an env var is allowed
// so the same binary can be pointed at staging and production blobs without
// a rebuild. That fallback must NOT survive into a shipped build: it lets
// anyone swap the trust root, which is the one input that must not be
// swappable.
inline std::string productDataBlob() {
#ifdef MC_NEXKEY_PRODUCT_DATA
  return std::string(MC_NEXKEY_PRODUCT_DATA);
#else
  const char *fromEnv = std::getenv("NEXKEYRUNTIME_PRODUCT_DATA");
  if (fromEnv && fromEnv[0] != '\0') return std::string(fromEnv);
  const char *fromFile = std::getenv("NEXKEYRUNTIME_PRODUCT_FILE");
  if (fromFile && fromFile[0] != '\0') {
    if (FILE *handle = std::fopen(fromFile, "rb")) {
      std::string text;
      char buffer[1024];
      std::size_t read = 0;
      while ((read = std::fread(buffer, 1, sizeof(buffer), handle)) > 0) {
        text.append(buffer, read);
      }
      std::fclose(handle);
      while (!text.empty() && (text.back() == '\n' || text.back() == '\r' ||
                               text.back() == ' ')) {
        text.pop_back();
      }
      return text;
    }
  }
  return std::string();
#endif
}

inline const char *editionName(NexKeyRuntimeEdition edition) {
  switch (edition) {
    case NEXKEYRUNTIME_EDITION_DEMO: return "demo";
    case NEXKEYRUNTIME_EDITION_TRIAL: return "trial";
    case NEXKEYRUNTIME_EDITION_BETA: return "beta";
    case NEXKEYRUNTIME_EDITION_FULL: return "full";
    case NEXKEYRUNTIME_EDITION_UNKNOWN: break;
  }
  return "UNKNOWN";
}

inline std::string formatTime(std::int64_t unixSeconds) {
  if (unixSeconds <= 0) return "-";
  const std::time_t raw = static_cast<std::time_t>(unixSeconds);
  std::tm parts{};
#if defined(_WIN32)
  localtime_s(&parts, &raw);
#else
  localtime_r(&raw, &parts);
#endif
  char buffer[32];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &parts) == 0) {
    return "-";
  }
  return std::string(buffer);
}

inline const char *statusName(NexKeyRuntimeLicenseStatus status) {
  switch (status) {
    case NEXKEYRUNTIME_LICENSE_NOT_ACTIVATED: return "NOT_ACTIVATED";
    case NEXKEYRUNTIME_LICENSE_ACTIVATING: return "ACTIVATING";
    case NEXKEYRUNTIME_LICENSE_ACTIVE: return "ACTIVE";
    case NEXKEYRUNTIME_LICENSE_OFFLINE_GRACE: return "OFFLINE_GRACE";
    case NEXKEYRUNTIME_LICENSE_OFFLINE_GRACE_EXPIRED: return "OFFLINE_GRACE_EXPIRED";
    case NEXKEYRUNTIME_LICENSE_EXPIRED: return "EXPIRED";
    case NEXKEYRUNTIME_LICENSE_SUSPENDED: return "SUSPENDED";
    case NEXKEYRUNTIME_LICENSE_REVOKED: return "REVOKED";
    case NEXKEYRUNTIME_LICENSE_ACTIVATION_REMOVED: return "ACTIVATION_REMOVED";
    case NEXKEYRUNTIME_LICENSE_DEVICE_MISMATCH: return "DEVICE_MISMATCH";
    case NEXKEYRUNTIME_LICENSE_CERTIFICATE_INVALID: return "CERTIFICATE_INVALID";
    case NEXKEYRUNTIME_LICENSE_CLOCK_ROLLBACK: return "CLOCK_ROLLBACK";
    case NEXKEYRUNTIME_LICENSE_SERVICE_UNAVAILABLE: return "SERVICE_UNAVAILABLE";
    case NEXKEYRUNTIME_LICENSE_INTERNAL_ERROR: return "INTERNAL_ERROR";
    case NEXKEYRUNTIME_LICENSE_UNKNOWN: break;
  }
  return "UNKNOWN";
}

// Recomputed rather than read from the SDK, which does not publish its
// storage path. That is the point for P11: this is the path THIS PROCESS
// resolves, and comparing it against the one nexkeyctl printed outside the
// host is the whole experiment. A sandboxed DaVinci Resolve redirects
// ~/Library/Application Support into its own container, and the two strings
// come out different (§3.12).
inline std::string receiptsDirectory() {
#if defined(_WIN32)
  const char *base = std::getenv("LOCALAPPDATA");
  if (!base || base[0] == '\0') return std::string();
  return std::string(base) + "\\NexKeyRuntime\\" MC_NEXKEY_TENANT "\\receipts";
#else
  const char *home = std::getenv("HOME");
  if (!home || home[0] == '\0') return std::string();
  return std::string(home) +
         "/Library/Application Support/NexKeyRuntime/" MC_NEXKEY_TENANT "/receipts";
#endif
}

// Counted, not merely "does the directory exist" — that always answers yes.
// resolveTenantPaths() creates the chain with mkdir -p semantics, so a
// sandboxed host builds its own redirected copy and looks perfectly healthy.
// The path plus this number is what carries the answer.
inline int countReceipts(const std::string &path) {
  if (path.empty()) return -1;
#if defined(_WIN32)
  WIN32_FIND_DATAA data{};
  const std::string pattern = path + "\\*.json";
  HANDLE find = FindFirstFileA(pattern.c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) return 0;
  int found = 0;
  do { ++found; } while (FindNextFileA(find, &data));
  FindClose(find);
  return found;
#else
  DIR *dir = ::opendir(path.c_str());
  if (!dir) return -1;
  int found = 0;
  while (struct dirent *entry = ::readdir(dir)) {
    const std::string name(entry->d_name);
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0) {
      ++found;
    }
  }
  ::closedir(dir);
  return found;
#endif
}

// Owns the handle for the lifetime of one plugin instance. Non-copyable: two
// owners would double-destroy, and destroy() is the call that joins the SDK's
// background thread — doing it twice inside a host is not a leak, it is a
// crash.
class License {
public:
  License() = default;
  ~License() { shutdown(); }
  License(const License &) = delete;
  License &operator=(const License &) = delete;

  // Called once, off the render thread. Everything expensive — signature
  // verification, disk reads — happens here so the render path only ever
  // does an atomic load (§7.3).
  void start() {
    if (handle_) return;
    handle_ = nexkeyruntime_license_create();
    if (!handle_) return;

    // Subscribing is what makes the background poller VISIBLE. Without it a
    // sync is invisible from the outside: it rewrites the receipt on disk and
    // nothing on screen changes until something re-reads it. The counter also
    // proves the poller is running at all, which is otherwise only observable
    // by waiting out syncAfter (24h) and watching network traffic.
    //
    // Runs on the poller thread. It records and returns — no work, no calls
    // back into the handle, which the ABI documents as forbidden here.
    nexkeyruntime_license_set_callback(handle_, &License::onSync, this);

    const std::string blob = productDataBlob();
    if (blob.empty() ||
        nexkeyruntime_license_set_product_data(handle_, blob.c_str()) !=
            NEXKEYRUNTIME_OK) {
      configured_ = false;
      return;
    }
    nexkeyruntime_license_set_tenant_id(handle_, MC_NEXKEY_TENANT);
    nexkeyruntime_license_set_variant(handle_, MC_NEXKEY_VARIANT);
    nexkeyruntime_license_set_metadata(handle_, "appVersion", PLUGIN_VERSION);
    // Ignored by SDKs older than 0.4.0, which accept unknown metadata keys
    // rather than rejecting them — so this is safe to set regardless of which
    // vendored runtime this build links.
    nexkeyruntime_license_set_metadata(handle_, "product", MC_NEXKEY_PRODUCT);
    configured_ = true;
    refresh();
  }

  void shutdown() {
    if (!handle_) return;
    // Returns only once the SDK's background thread has been joined, so the
    // host may unload this bundle the moment it returns.
    nexkeyruntime_license_destroy(handle_);
    handle_ = nullptr;
  }

  // Re-reads the receipt from disk. Safe to call from the UI thread; never
  // from render.
  void refresh() {
    if (!handle_ || !configured_) return;
    nexkeyruntime_license_load_local(handle_);
  }

  // Blocking, on purpose: it is wired to a button, and the operator wants the
  // answer before the panel refreshes. Never call it from render.
  //
  // Deliberately does NOT call load_local() afterwards. request_sync() already
  // re-evaluates and then writes the seat counts the server reported; a
  // load_local() on top rebuilds the snapshot from the receipt alone and
  // silently throws those away, which is exactly what made the panel keep
  // saying "seats unknown" immediately after a successful sync.
  void syncNow() {
    if (!handle_ || !configured_) return;

    // request_sync is only synchronous when no poller exists. Once one is
    // running — which it is, on any interactive host with a stored key — the
    // call hands the work to that thread and returns immediately, so reading
    // the snapshot right after shows the state from BEFORE the sync. That is
    // what made a successful "Sync Now" still report "seats unknown".
    //
    // So wait for the poller to publish, bounded. The wait belongs here and
    // not in the callback because OFX parameters may only be written from the
    // UI thread, and the callback runs on the poller's.
    const unsigned long before = syncCount_.load(std::memory_order_acquire);
    const NexKeyRuntimeResult result =
        nexkeyruntime_license_request_sync(handle_, 1);
    manualSyncs_.fetch_add(1, std::memory_order_acq_rel);
    lastSyncOk_.store(result == NEXKEYRUNTIME_OK, std::memory_order_release);

    if (result == NEXKEYRUNTIME_OK) {
      // ~4s ceiling: longer than a healthy round trip, short enough that a
      // stuck backend does not appear to freeze the host's UI.
      for (int attempt = 0; attempt < 80; ++attempt) {
        if (syncCount_.load(std::memory_order_acquire) != before) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
    lastSyncAt_.store(static_cast<long long>(std::time(nullptr)),
                      std::memory_order_release);
  }

  // THE hot path. One atomic load inside the SDK, no I/O, no lock — safe to
  // call per frame.
  //
  // In shadow mode this is compiled down to a constant true and the SDK is
  // not consulted at all, so the render path costs exactly what it did before
  // licensing existed.
  bool allowed() const {
#if MC_NEXKEY_ENFORCE
    if (!handle_) return true; // licensing not compiled in: never gate
    return nexkeyruntime_license_render_decision(handle_) ==
           NEXKEYRUNTIME_RENDER_ALLOW;
#else
    return true;
#endif
  }

  // The verdict the license actually carries, regardless of whether it is
  // being enforced. This is what the UI shows; allowed() is what the render
  // path obeys. They differ on purpose in shadow mode, and the UI says so.
  bool licenseAllows() const {
    if (!handle_) return true;
    return nexkeyruntime_license_render_decision(handle_) ==
           NEXKEYRUNTIME_RENDER_ALLOW;
  }

  LicenseReport report() const {
    LicenseReport out;
    out.enforcing = MC_NEXKEY_ENFORCE ? true : false;
    out.receiptsDir = receiptsDirectory();
    out.receiptsFound = countReceipts(out.receiptsDir);
    if (!handle_) { out.status = "no handle"; return out; }
    if (!configured_) { out.status = "PRODUCT_DATA MISSING/INVALID"; return out; }

    NexKeyRuntimeLicenseSnapshot snapshot{};
    snapshot.struct_size = sizeof(snapshot);
    if (nexkeyruntime_license_get_snapshot(handle_, &snapshot) != NEXKEYRUNTIME_OK) {
      out.status = "no snapshot";
      return out;
    }

    out.status = statusName(snapshot.status);
    out.allowed = licenseAllows();

    {
      const unsigned long background = syncCount_.load(std::memory_order_acquire);
      const unsigned long manual = manualSyncs_.load(std::memory_order_acquire);
      const unsigned long count = background + manual;
      // snapshot.last_synced_at is the SDK's own answer and is preferred, but
      // NexKeyRuntime 0.2.0 declares that field without ever writing it — it
      // is always 0 there. The locally observed timestamp is the fallback so
      // this panel works against the vendored release rather than waiting for
      // the fix to ship.
      const long long observedAt = lastSyncAt_.load(std::memory_order_acquire);
      const std::int64_t when =
          snapshot.last_synced_at != 0 ? snapshot.last_synced_at
                                       : static_cast<std::int64_t>(observedAt);
      if (count == 0 && when == 0) {
        out.sync = "none yet   (poller next: see Validity)";
      } else {
        // Counted separately because they answer different questions: manual
        // syncs prove the network path works, background ones prove the
        // POLLER is running, which is the part nothing else on screen shows.
        out.sync = std::to_string(background) + " background, " +
                   std::to_string(manual) + " manual";
        if (when != 0) out.sync += "   last " + formatTime(when);
        const int last = lastSyncStatus_.load(std::memory_order_acquire);
        if (last >= 0) {
          out.sync += "   -> ";
          out.sync += statusName(static_cast<NexKeyRuntimeLicenseStatus>(last));
        } else if (manual != 0) {
          out.sync += lastSyncOk_.load(std::memory_order_acquire) ? "   -> OK"
                                                                 : "   -> FAILED";
        }
      }
    }

    // With no receipt there are no claims to describe, and the zeroed
    // snapshot would otherwise render as "perpetual" with a blank edition —
    // a licence that does not exist reading as one that never expires.
    if (snapshot.activation_id[0] == '\0') {
      out.edition = "-";
      out.validity = "-";
      out.activation = "-";
      return out;
    }

    // The edition label is shown verbatim next to the enum the SDK mapped it
    // to. They are printed separately on purpose: the SDK never interprets
    // the label (§3.7.2), so a tenant introducing an edition this build has
    // not heard of shows its real name with UNKNOWN beside it, rather than
    // being silently flattened into an existing one.
    out.edition = snapshot.edition[0] ? snapshot.edition : "(none)";
    out.edition += "  [";
    out.edition += editionName(snapshot.edition_enum);
    out.edition += "]";

    out.validity = snapshot.expires_at == 0
                       ? std::string("perpetual")
                       : (formatTime(snapshot.expires_at) + "  (" +
                          std::to_string(snapshot.days_remaining) + " day(s) left)");
    if (snapshot.offline_valid_until != 0) {
      out.validity += "   offline until " + formatTime(snapshot.offline_valid_until);
    }
    if (snapshot.sync_after != 0) {
      out.validity += "   next sync " + formatTime(snapshot.sync_after);
    }

    out.activation = snapshot.activation_id[0] ? snapshot.activation_id : "(none)";
    if (snapshot.max_activations > 0) {
      out.activation += "   seats " + std::to_string(snapshot.activations_used) +
                        "/" + std::to_string(snapshot.max_activations);
    } else {
      // Seat counts come from the server and are not in the receipt, so a
      // local read genuinely does not know them. "0/0" would read as "no
      // seats in use", which is wrong and alarming.
      out.activation += "   seats unknown (sync to refresh)";
    }
    return out;
  }

private:
  static void onSync(NexKeyRuntimeLicenseStatus status, void *userData) {
    if (License *self = static_cast<License *>(userData)) {
      self->syncCount_.fetch_add(1, std::memory_order_acq_rel);
      self->lastSyncStatus_.store(static_cast<int>(status), std::memory_order_release);
      self->lastSyncAt_.store(static_cast<long long>(std::time(nullptr)),
                              std::memory_order_release);
    }
  }

  NexKeyRuntimeLicenseHandle *handle_ = nullptr;
  bool configured_ = false;
  std::atomic<unsigned long> syncCount_{0};
  std::atomic<unsigned long> manualSyncs_{0};
  std::atomic<bool> lastSyncOk_{false};
  std::atomic<int> lastSyncStatus_{-1};
  std::atomic<long long> lastSyncAt_{0};
};

#else // MC_NEXKEY_ENABLED

// Licensing compiled out. allowed() is unconditionally true: a build without
// the SDK must behave exactly as the plugin did before it existed, never fail
// closed on an integration that is not there.
class License {
public:
  void start() {}
  void shutdown() {}
  void refresh() {}
  void syncNow() {}
  bool allowed() const { return true; }

  LicenseReport report() const {
    LicenseReport out;
    // `allowed` MUST agree with allowed() above. The first version of this
    // returned a default-constructed report, whose `allowed` is false, so a
    // build with licensing compiled out rendered normally while its own UI
    // announced "(deny)" — a diagnostic that lies is worse than none, and
    // this one exists precisely to disambiguate why a plugin is denied.
    out.allowed = true;
    out.enforcing = false;
    out.status = "not compiled in — build with NEXKEY_ROOT";
    return out;
  }
};

#endif // MC_NEXKEY_ENABLED

} // namespace mc

#endif // MC_LICENSE_H
