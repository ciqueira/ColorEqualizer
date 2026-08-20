#ifndef NEXKEYRUNTIME_NEXKEYRUNTIME_H
#define NEXKEYRUNTIME_NEXKEYRUNTIME_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(NEXKEYRUNTIME_SHARED)
#  if defined(NEXKEYRUNTIME_BUILDING_LIBRARY)
#    define NEXKEYRUNTIME_API __declspec(dllexport)
#  else
#    define NEXKEYRUNTIME_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define NEXKEYRUNTIME_API __attribute__((visibility("default")))
#else
#  define NEXKEYRUNTIME_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Kept in step with MCSDK/VERSION, which is the file a human edits; CI fails
   the build if they disagree. These macros cannot simply be generated from it:
   they are public API (the package-consumer test asserts nexkeyruntime_version()
   equals NEXKEYRUNTIME_VERSION_STRING), and this header ships as SOURCE in the
   public repository rather than as a build artifact. */
#define NEXKEYRUNTIME_VERSION_MAJOR 0
#define NEXKEYRUNTIME_VERSION_MINOR 2
#define NEXKEYRUNTIME_VERSION_PATCH 3
#define NEXKEYRUNTIME_VERSION_STRING "0.2.3"

#define NEXKEYRUNTIME_VERSION_CAPACITY 64
#define NEXKEYRUNTIME_ID_CAPACITY 160
#define NEXKEYRUNTIME_TITLE_CAPACITY 192
#define NEXKEYRUNTIME_MESSAGE_CAPACITY 2048
#define NEXKEYRUNTIME_URL_CAPACITY 2048

typedef struct NexKeyRuntimeHandle NexKeyRuntimeHandle;

typedef enum NexKeyRuntimeResult {
  NEXKEYRUNTIME_OK = 0,
  NEXKEYRUNTIME_INVALID_ARGUMENT = 1,
  NEXKEYRUNTIME_INVALID_CONFIG = 2,
  NEXKEYRUNTIME_BUSY = 3,
  NEXKEYRUNTIME_NETWORK_ERROR = 4,
  NEXKEYRUNTIME_HTTP_ERROR = 5,
  NEXKEYRUNTIME_INVALID_MANIFEST = 6,
  NEXKEYRUNTIME_UNSUPPORTED_SCHEMA = 7,
  NEXKEYRUNTIME_NOT_AVAILABLE = 8,
  NEXKEYRUNTIME_BUFFER_TOO_SMALL = 9,
  NEXKEYRUNTIME_INTERNAL_ERROR = 10,

  /* Licensing (§7.7, Fase 4A) — appended starting at 20, never renumbering
     0-10 above: those shipped with the update handle in Fase 1. Where a
     meaning already exists here (invalid argument, internal error, network
     error, ...) licensing reuses it instead of declaring a second name for
     the same thing. */

  /* configuration */
  NEXKEYRUNTIME_E_PRODUCT_DATA = 20,        /* blob invalid or tampered */
  NEXKEYRUNTIME_E_PRODUCT_DATA_EXPIRED = 21,
  NEXKEYRUNTIME_E_ABI_MISMATCH = 22,        /* product data formatVersion this build does not understand */

  /* license — from the server; returned by Bloco B/C, not yet implemented (Fase 4B) */
  NEXKEYRUNTIME_E_LICENSE_KEY = 30,
  NEXKEYRUNTIME_E_ACTIVATION_LIMIT = 31,
  NEXKEYRUNTIME_E_LICENSE_EXPIRED = 32,
  NEXKEYRUNTIME_E_LICENSE_SUSPENDED = 33,
  NEXKEYRUNTIME_E_LICENSE_REVOKED = 34,
  NEXKEYRUNTIME_E_PRODUCT_ACTIVATED = 35,
  NEXKEYRUNTIME_E_WRONG_TENANT = 36,
  NEXKEYRUNTIME_E_ACTIVATION_NOT_FOUND = 37,

  /* local proof — from load_local() */
  NEXKEYRUNTIME_E_NO_RECEIPT = 40,
  NEXKEYRUNTIME_E_SIGNATURE = 41,
  NEXKEYRUNTIME_E_DEVICE_MISMATCH = 42,
  NEXKEYRUNTIME_E_ENTITLEMENT = 43,         /* a receipt verified but for a different variant */
  NEXKEYRUNTIME_E_CLOCK = 44,

  /* environment */
  NEXKEYRUNTIME_E_STORAGE = 50              /* tenant directory unreadable/uncreatable */
} NexKeyRuntimeResult;

typedef enum NexKeyRuntimeUpdateStatus {
  NEXKEYRUNTIME_UPDATE_IDLE = 0,
  NEXKEYRUNTIME_UPDATE_CHECKING = 1,
  NEXKEYRUNTIME_UPDATE_UP_TO_DATE = 2,
  NEXKEYRUNTIME_UPDATE_AVAILABLE = 3,
  NEXKEYRUNTIME_UPDATE_OFFLINE = 4,
  NEXKEYRUNTIME_UPDATE_ERROR = 5
} NexKeyRuntimeUpdateStatus;

typedef enum NexKeyRuntimeNoticeType {
  NEXKEYRUNTIME_NOTICE_UPDATE = 0,
  NEXKEYRUNTIME_NOTICE_COMPATIBILITY = 1,
  NEXKEYRUNTIME_NOTICE_SECURITY = 2,
  NEXKEYRUNTIME_NOTICE_MAINTENANCE = 3,
  NEXKEYRUNTIME_NOTICE_MIGRATION = 4,
  NEXKEYRUNTIME_NOTICE_DEPRECATION = 5
} NexKeyRuntimeNoticeType;

typedef enum NexKeyRuntimeNoticeSeverity {
  NEXKEYRUNTIME_NOTICE_INFO = 0,
  NEXKEYRUNTIME_NOTICE_RECOMMENDED = 1,
  NEXKEYRUNTIME_NOTICE_CRITICAL = 2
} NexKeyRuntimeNoticeSeverity;

typedef struct NexKeyRuntimeManifestRequest {
  size_t struct_size;
  const char *url;
  const char *tenant_id;
  const char *artifact_id;
  const char *platform;
  const char *architecture;
  const char *channel;
  const char *current_version;
  const char *sdk_version;
  const char *accept_language;
  const char *if_none_match;
  uint32_t timeout_ms;
} NexKeyRuntimeManifestRequest;

typedef struct NexKeyRuntimeManifestResponse {
  size_t struct_size;
  int http_status;
  const unsigned char *body;
  size_t body_size;
  const char *etag;
} NexKeyRuntimeManifestResponse;

typedef NexKeyRuntimeResult (*NexKeyRuntimeFetchManifestFn)(
  void *user_data,
  const NexKeyRuntimeManifestRequest *request,
  NexKeyRuntimeManifestResponse *response
);
typedef void (*NexKeyRuntimeCancelFetchFn)(void *user_data);

typedef int64_t (*NexKeyRuntimeNowFn)(void *user_data);
typedef int (*NexKeyRuntimeCanOpenUrlFn)(void *user_data, const char *url);
typedef int (*NexKeyRuntimeOpenUrlFn)(void *user_data, const char *url);

typedef struct NexKeyRuntimeConfig {
  size_t struct_size;
  const char *tenant_id;
  const char *artifact_id;
  const char *current_version;
  const char *channel;
  const char *platform;
  const char *architecture;
  const char *locale;
  const char *host_name;
  const char *host_version;
  const char *base_url;
  uint32_t request_timeout_ms;
  NexKeyRuntimeFetchManifestFn fetch_manifest;
  NexKeyRuntimeCancelFetchFn cancel_fetch;
  NexKeyRuntimeNowFn now;
  NexKeyRuntimeCanOpenUrlFn can_open_url;
  NexKeyRuntimeOpenUrlFn open_url;
  void *user_data;
} NexKeyRuntimeConfig;

typedef struct NexKeyRuntimeUpdateSnapshot {
  size_t struct_size;
  NexKeyRuntimeUpdateStatus status;
  NexKeyRuntimeResult last_result;
  int has_checked;
  int has_update;
  int64_t checked_at;
  int64_t next_check_at;
  char current_version[NEXKEYRUNTIME_VERSION_CAPACITY];
  char latest_version[NEXKEYRUNTIME_VERSION_CAPACITY];
  char release_id[NEXKEYRUNTIME_ID_CAPACITY];
  char notes_url[NEXKEYRUNTIME_URL_CAPACITY];
  char deep_link[NEXKEYRUNTIME_URL_CAPACITY];
  char fallback_url[NEXKEYRUNTIME_URL_CAPACITY];
} NexKeyRuntimeUpdateSnapshot;

typedef struct NexKeyRuntimeNotice {
  size_t struct_size;
  int available;
  int revision;
  int dismissible;
  int64_t starts_at;
  int64_t expires_at;
  int64_t remind_after_seconds;
  NexKeyRuntimeNoticeType type;
  NexKeyRuntimeNoticeSeverity severity;
  char id[NEXKEYRUNTIME_ID_CAPACITY];
  char title[NEXKEYRUNTIME_TITLE_CAPACITY];
  char message[NEXKEYRUNTIME_MESSAGE_CAPACITY];
  char deep_link[NEXKEYRUNTIME_URL_CAPACITY];
  char fallback_url[NEXKEYRUNTIME_URL_CAPACITY];
} NexKeyRuntimeNotice;

NEXKEYRUNTIME_API void nexkeyruntime_config_init(NexKeyRuntimeConfig *config);
NEXKEYRUNTIME_API const char *nexkeyruntime_version(void);

NEXKEYRUNTIME_API NexKeyRuntimeHandle *nexkeyruntime_create(
  const NexKeyRuntimeConfig *config
);
NEXKEYRUNTIME_API void nexkeyruntime_destroy(NexKeyRuntimeHandle *handle);

NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_request_check(
  NexKeyRuntimeHandle *handle,
  int force
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_get_snapshot(
  NexKeyRuntimeHandle *handle,
  NexKeyRuntimeUpdateSnapshot *out
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_get_active_notice(
  NexKeyRuntimeHandle *handle,
  NexKeyRuntimeNotice *out
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_dismiss_notice(
  NexKeyRuntimeHandle *handle,
  const char *id,
  int revision
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_remind_notice_later(
  NexKeyRuntimeHandle *handle,
  const char *id
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_open_update_action(
  NexKeyRuntimeHandle *handle
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_open_notice_action(
  NexKeyRuntimeHandle *handle,
  const char *id
);

/* =================================================================
   LICENSING (opt-in, §7.7) — a second, independent handle. An app
   can use the update handle above, this one, or both; neither knows
   about the other.

   New NexKeyRuntimeResult values are appended starting at 20, never
   renumbering 0-10 above: those already shipped with the update
   handle in Fase 1, and where a meaning already exists there
   (invalid argument, internal error, network error, ...) licensing
   reuses it rather than declaring a second name for the same thing.
   ================================================================= */

typedef struct NexKeyRuntimeLicenseHandle NexKeyRuntimeLicenseHandle;

typedef enum NexKeyRuntimeRenderDecision {
  NEXKEYRUNTIME_RENDER_DENY = 0,
  NEXKEYRUNTIME_RENDER_ALLOW = 1
} NexKeyRuntimeRenderDecision;

/* Mirrors nexkeyruntime::licensing::LicenseStatus (LicenseStatus.h)
   field-for-field; ActivationCertificateVerifier/LicenseStateMachine
   are the only place these are DECIDED, this enum only reports one.
   Local verification (load_local) only ever produces a subset of these —
   see LicenseStatus.h for exactly which. */
typedef enum NexKeyRuntimeLicenseStatus {
  NEXKEYRUNTIME_LICENSE_UNKNOWN = 0,
  NEXKEYRUNTIME_LICENSE_NOT_ACTIVATED,
  NEXKEYRUNTIME_LICENSE_ACTIVATING,
  NEXKEYRUNTIME_LICENSE_ACTIVE,
  NEXKEYRUNTIME_LICENSE_OFFLINE_GRACE,
  NEXKEYRUNTIME_LICENSE_OFFLINE_GRACE_EXPIRED,
  NEXKEYRUNTIME_LICENSE_EXPIRED,
  NEXKEYRUNTIME_LICENSE_SUSPENDED,
  NEXKEYRUNTIME_LICENSE_REVOKED,
  NEXKEYRUNTIME_LICENSE_ACTIVATION_REMOVED,
  NEXKEYRUNTIME_LICENSE_DEVICE_MISMATCH,
  NEXKEYRUNTIME_LICENSE_CERTIFICATE_INVALID,
  NEXKEYRUNTIME_LICENSE_CLOCK_ROLLBACK,
  NEXKEYRUNTIME_LICENSE_SERVICE_UNAVAILABLE,
  NEXKEYRUNTIME_LICENSE_INTERNAL_ERROR
} NexKeyRuntimeLicenseStatus;

/* UNKNOWN = 0 so a future edition this build has never heard of does not
   silently misreport as any specific existing one (§3.7.3). `edition` (the
   string) is the authority; `edition_enum` is a convenience for callers who
   would rather switch on an int. */
typedef enum NexKeyRuntimeEdition {
  NEXKEYRUNTIME_EDITION_UNKNOWN = 0,
  NEXKEYRUNTIME_EDITION_BETA,
  NEXKEYRUNTIME_EDITION_DEMO,
  NEXKEYRUNTIME_EDITION_TRIAL,
  NEXKEYRUNTIME_EDITION_FULL
} NexKeyRuntimeEdition;

typedef struct NexKeyRuntimeLicenseSnapshot {
  size_t struct_size;
  NexKeyRuntimeLicenseStatus status;
  NexKeyRuntimeRenderDecision decision;
  NexKeyRuntimeResult last_error;

  int64_t activated_at;

  /* the only axis the SDK itself imposes (§3.7.5) */
  int64_t expires_at;    /* 0 = perpetual */
  int32_t days_remaining; /* -1 if perpetual */

  int64_t sync_after;         /* Fase 4B: when the poller would next try */
  int64_t offline_valid_until;
  int64_t last_synced_at;     /* Fase 4B: 0 until a sync has happened */

  int32_t activations_used;   /* Fase 4B: 0 until reported by a sync */
  int32_t max_activations;

  char edition[NEXKEYRUNTIME_VERSION_CAPACITY];   /* label; SDK never interprets it (§3.7.2) */
  NexKeyRuntimeEdition edition_enum;
  char activation_id[NEXKEYRUNTIME_ID_CAPACITY];
  char certificate_id[NEXKEYRUNTIME_ID_CAPACITY];
} NexKeyRuntimeLicenseSnapshot;

NEXKEYRUNTIME_API NexKeyRuntimeLicenseHandle *nexkeyruntime_license_create(void);
NEXKEYRUNTIME_API void nexkeyruntime_license_destroy(NexKeyRuntimeLicenseHandle *handle);

/* ---- configuration (§3.14): one call for the common case ---- */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_product_data(
  NexKeyRuntimeLicenseHandle *handle,
  const char *product_data
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_product_file(
  NexKeyRuntimeLicenseHandle *handle,
  const char *path
);
/* Mandatory (§3.7.1.1.1) — not inside product_data. Must be called before
   load_local(); load_local() fails NEXKEYRUNTIME_INVALID_CONFIG without it. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_tenant_id(
  NexKeyRuntimeLicenseHandle *handle,
  const char *tenant_id
);
/* Optional — a tenant with exactly one variant never needs this; the
   entitlement checked defaults to "download:default" if unset. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_variant(
  NexKeyRuntimeLicenseHandle *handle,
  const char *variant
);

/* ===== BLOCO A — VERIFY: the four (after create/destroy) every OFX
   uses. A plugin that never calls anything below this point is,
   by construction, Perfil A/read-only — see §7.7. ===== */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_load_local(
  NexKeyRuntimeLicenseHandle *handle
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_get_snapshot(
  NexKeyRuntimeLicenseHandle *handle,
  NexKeyRuntimeLicenseSnapshot *out
);
/* HOT PATH: one atomic load, no I/O, no lock. Safe to call from the render
   thread on every frame. */
NEXKEYRUNTIME_API NexKeyRuntimeRenderDecision nexkeyruntime_license_render_decision(
  const NexKeyRuntimeLicenseHandle *handle
);

/* ===== BLOCO B — ACTIVATE: MCNexus (Perfil A) or the app itself
   (Perfil B). A plugin that only validates never calls these (D15). ===== */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_license_key(
  NexKeyRuntimeLicenseHandle *handle,
  const char *key
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_metadata(
  NexKeyRuntimeLicenseHandle *handle,
  const char *key,
  const char *value
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_activate(
  NexKeyRuntimeLicenseHandle *handle
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_deactivate(
  NexKeyRuntimeLicenseHandle *handle
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_publish_receipt(
  NexKeyRuntimeLicenseHandle *handle,
  const char *certificate
);

/* ===== BLOCO C — SYNC: automatic once Fase 4B's poller runs; forcing
   is optional. ===== */

/* Invoked on the POLLER thread, never on the render thread, every time a
   background sync completes. Modelled on LexActivator's SetLicenseCallback,
   which exists for the same reason: without it an integrator learns about a
   revocation only by polling the SDK, which is the loop the SDK already runs
   on their behalf (D36).

   The callback must not block and must not call back into this handle — it is
   a notification, not a place to do work. Read the snapshot afterwards from
   your own thread. */
typedef void (*NexKeyRuntimeLicenseCallback)(
  NexKeyRuntimeLicenseStatus status,
  void *user_data
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_callback(
  NexKeyRuntimeLicenseHandle *handle,
  NexKeyRuntimeLicenseCallback callback,
  void *user_data
);

NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_request_sync(
  NexKeyRuntimeLicenseHandle *handle,
  int force
);
/* Optional; overrides autodetection in either direction (§7.9.5, D7) — the
   default matters because assuming GUI would let an unattended render farm
   hammer the backend, and assuming headless would mean a normal desktop user
   never syncs. Autodetection errs toward interactive when it cannot tell. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_headless(
  NexKeyRuntimeLicenseHandle *handle,
  int headless
);

#ifdef __cplusplus
}
#endif

#endif
