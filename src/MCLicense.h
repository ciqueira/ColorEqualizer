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

#include <string>

#ifdef MC_NEXKEY_ENABLED
#include <nexkeyruntime/nexkeyruntime.h>
#include <cstdio>
#include <cstdlib>
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
  std::string receiptsDir;
  int receiptsFound = -1;
  bool allowed = false;
};

#ifdef MC_NEXKEY_ENABLED

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

  // THE hot path. One atomic load inside the SDK, no I/O, no lock — safe to
  // call per frame.
  bool allowed() const {
    if (!handle_) return true; // licensing not compiled in: never gate
    return nexkeyruntime_license_render_decision(handle_) ==
           NEXKEYRUNTIME_RENDER_ALLOW;
  }

  LicenseReport report() const {
    LicenseReport out;
    out.receiptsDir = receiptsDirectory();
    out.receiptsFound = countReceipts(out.receiptsDir);
    if (!handle_) { out.status = "no handle"; return out; }
    if (!configured_) { out.status = "PRODUCT_DATA MISSING/INVALID"; return out; }

    NexKeyRuntimeLicenseSnapshot snapshot{};
    snapshot.struct_size = sizeof(snapshot);
    if (nexkeyruntime_license_get_snapshot(handle_, &snapshot) == NEXKEYRUNTIME_OK) {
      out.status = statusName(snapshot.status);
    } else {
      out.status = "no snapshot";
    }
    out.allowed = allowed();
    return out;
  }

private:
  NexKeyRuntimeLicenseHandle *handle_ = nullptr;
  bool configured_ = false;
};

#else // MC_NEXKEY_ENABLED

// Licensing compiled out. allowed() is unconditionally true: a build without
// the SDK must behave exactly as the plugin did before it existed, never
// fail closed on an integration that is not there.
class License {
public:
  void start() {}
  void shutdown() {}
  void refresh() {}
  bool allowed() const { return true; }
  LicenseReport report() const { return LicenseReport(); }
};

#endif // MC_NEXKEY_ENABLED

} // namespace mc

#endif // MC_LICENSE_H
