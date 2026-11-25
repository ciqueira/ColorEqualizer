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

/* Kept in step with the SDK release version; CI fails
   the build if they disagree. These macros cannot simply be generated from it:
   they are public API (the package-consumer test asserts nexkeyruntime_version()
   equals NEXKEYRUNTIME_VERSION_STRING), and this header ships as SOURCE in the
   public repository rather than as a build artifact. */
#define NEXKEYRUNTIME_VERSION_MAJOR 1
#define NEXKEYRUNTIME_VERSION_MINOR 4
#define NEXKEYRUNTIME_VERSION_PATCH 6
#define NEXKEYRUNTIME_VERSION_STRING "1.4.6"

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

  /* Licensing codes: appended starting at 20 without renumbering 0-10 above.
     Where an existing status code matches (invalid argument, internal error,
     network error, ...), licensing reuses it. */

  /* configuration */
  NEXKEYRUNTIME_E_PRODUCT_DATA = 20,        /* blob invalid or tampered */
  NEXKEYRUNTIME_E_PRODUCT_DATA_EXPIRED = 21,
  NEXKEYRUNTIME_E_ABI_MISMATCH = 22,        /* product data formatVersion this build does not understand */

  /* server-reported license errors (Profile B / background sync) */
  NEXKEYRUNTIME_E_LICENSE_KEY = 30,
  NEXKEYRUNTIME_E_ACTIVATION_LIMIT = 31,
  NEXKEYRUNTIME_E_LICENSE_EXPIRED = 32,
  NEXKEYRUNTIME_E_LICENSE_SUSPENDED = 33,
  NEXKEYRUNTIME_E_LICENSE_REVOKED = 34,
  NEXKEYRUNTIME_E_PRODUCT_ACTIVATED = 35,
  NEXKEYRUNTIME_E_WRONG_TENANT = 36,
  NEXKEYRUNTIME_E_ACTIVATION_NOT_FOUND = 37,

  /* local proof: from load_local() */
  NEXKEYRUNTIME_E_NO_RECEIPT = 40,
  NEXKEYRUNTIME_E_SIGNATURE = 41,
  NEXKEYRUNTIME_E_DEVICE_MISMATCH = 42,
  NEXKEYRUNTIME_E_ENTITLEMENT = 43,         /* a receipt verified but for a different variant */
  NEXKEYRUNTIME_E_CLOCK = 44,

  /* environment */
  NEXKEYRUNTIME_E_STORAGE = 50,             /* tenant directory unreadable/uncreatable */

  /* asset vault */
  NEXKEYRUNTIME_E_ASSET_INVALID = 60,
  NEXKEYRUNTIME_E_ASSET_SIGNATURE = 61,
  NEXKEYRUNTIME_E_ASSET_CORRUPT = 62,
  NEXKEYRUNTIME_E_ASSET_DECRYPT_FAILED = 63,
  NEXKEYRUNTIME_E_ASSET_UNSUPPORTED_VERSION = 64,
  NEXKEYRUNTIME_E_ASSET_BACKEND_NOT_FOUND = 65,
  NEXKEYRUNTIME_E_ASSET_TOO_LARGE = 66,
  NEXKEYRUNTIME_E_ASSET_KEY_NOT_FOUND = 67
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

typedef enum NexKeyRuntimeScope {
  NEXKEYRUNTIME_SCOPE_USER = 0,
  NEXKEYRUNTIME_SCOPE_SYSTEM = 1
} NexKeyRuntimeScope;

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
   LICENSING: a second, independent handle. An app can use the
   update handle above, this one, or both; neither knows about the
   other.

   New NexKeyRuntimeResult values are appended starting at 20 without
   renumbering 0-10 above. Where an existing status code matches
   (invalid argument, internal error, network error, ...), licensing
   reuses it instead of declaring a second name.
   ================================================================= */

typedef struct NexKeyRuntimeLicenseHandle NexKeyRuntimeLicenseHandle;

typedef enum NexKeyRuntimeRenderDecision {
  NEXKEYRUNTIME_RENDER_DENY = 0,
  NEXKEYRUNTIME_RENDER_ALLOW = 1
} NexKeyRuntimeRenderDecision;

/* License status reported in snapshots. Local verification (load_local)
   produces only the offline-verifiable subset (e.g. NOT_ACTIVATED,
   ACTIVE, OFFLINE_GRACE, OFFLINE_GRACE_EXPIRED, EXPIRED,
   DEVICE_MISMATCH, CERTIFICATE_INVALID, CLOCK_ROLLBACK). */
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
   silently misreport as any existing one. `edition` (the string) is the
   authority; `edition_enum` is a convenience for callers who prefer
   switching on an integer. */
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

  int64_t expires_at;    /* 0 = perpetual */
  int32_t days_remaining; /* -1 if perpetual */

  int64_t sync_after;         /* when the poller would next try */
  int64_t offline_valid_until;
  int64_t last_synced_at;     /* 0 until a sync has happened */

  int32_t activations_used;   /* 0 until reported by a sync */
  int32_t max_activations;

  char edition[NEXKEYRUNTIME_VERSION_CAPACITY];   /* opaque label; not interpreted by SDK */
  NexKeyRuntimeEdition edition_enum;
  char activation_id[NEXKEYRUNTIME_ID_CAPACITY];
  char certificate_id[NEXKEYRUNTIME_ID_CAPACITY];
} NexKeyRuntimeLicenseSnapshot;

NEXKEYRUNTIME_API NexKeyRuntimeLicenseHandle *nexkeyruntime_license_create(void);
NEXKEYRUNTIME_API void nexkeyruntime_license_destroy(NexKeyRuntimeLicenseHandle *handle);

/* ---- configuration: one call for the common case ---- */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_product_data(
  NexKeyRuntimeLicenseHandle *handle,
  const char *product_data
);
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_product_file(
  NexKeyRuntimeLicenseHandle *handle,
  const char *path
);
/* Mandatory: not stored inside product_data. Must be called before
   load_local(); load_local() returns NEXKEYRUNTIME_INVALID_CONFIG without it. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_tenant_id(
  NexKeyRuntimeLicenseHandle *handle,
  const char *tenant_id
);
/* Controls whether the license state is stored and read from a user-specific
   profile location (USER) or a machine-wide shared location (SYSTEM). 
   Must be called before load_local() or activate(). Defaults to USER. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_scope(
  NexKeyRuntimeLicenseHandle *handle,
  NexKeyRuntimeScope scope
);
/* Optional: a tenant with exactly one variant never needs this; the
   entitlement checked defaults to "download:default" if unset. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_variant(
  NexKeyRuntimeLicenseHandle *handle,
  const char *variant
);

/* ===== PROFILE A: VERIFY: the calls every plugin uses. A plugin that
   never calls the activation block below operates as read-only Profile A. ===== */
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

/* ===== PROFILE B: ACTIVATE: MCNexus (Profile A) or the app itself
   (Profile B). A plugin that only validates never calls these. ===== */
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

/* Writes a small JSON request file that lets someone else issue this
   machine an activation certificate without this machine ever reaching the
   network (e.g. an unreachable backend or air-gapped installation). The file
   carries this machine's derived binding (never raw hardware identifiers),
   the entitlement, and a short prefix of the license key if configured on
   this handle. It contains no secret.

   Requires set_product_data/set_product_file and set_tenant_id to have
   already succeeded (same precondition as load_local()); overwrites `path`
   if it exists. Fails with NEXKEYRUNTIME_INVALID_CONFIG if that
   precondition is not met, NEXKEYRUNTIME_INTERNAL_ERROR if the machine
   binding cannot be derived, or NEXKEYRUNTIME_E_STORAGE if `path` cannot
   be written.

   Whatever comes back in response to this file is a certificate: hand it
   to publish_receipt(), not to this function. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_export_activation_request(
  NexKeyRuntimeLicenseHandle *handle,
  const char *path
);

/* The other half of the offline round trip: stops this machine's license
   and writes the deactivation proof to `path`, without touching the network.

   Local receipts are cleared BEFORE the proof is written so a deactivated
   machine cannot continue rendering. This ordering is enforced and not
   configurable; forward the generated file unmodified to release the seat.

   Same preconditions as export_activation_request(). Returns
   NEXKEYRUNTIME_E_NO_RECEIPT when there is no active receipt to release. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_export_deactivation_proof(
  NexKeyRuntimeLicenseHandle *handle,
  const char *path
);

/* ===== SYNC: automatic background refresh when active; forcing is optional. ===== */

/* Invoked on the poller thread, never on the render thread, every time a
   background sync completes so an integrator learns about a revocation or
   status change without polling the SDK.

   The callback must not block and must not call back into this handle; read
   the updated snapshot afterwards from your own thread. */
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
/* Optional; overrides headless autodetection in either direction so
   unattended render nodes stay offline-only while interactive sessions sync
   normally. Autodetection defaults toward interactive when uncertain. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_set_headless(
  NexKeyRuntimeLicenseHandle *handle,
  int headless
);

/* ===== SHADER VAULT & DCTL RUNNER API (v1.3.0) ===== */

typedef enum NexKeyRuntimeBackendId {
  NEXKEYRUNTIME_BACKEND_METAL_SRC  = 2,
  NEXKEYRUNTIME_BACKEND_METAL_BIN  = 3,
  NEXKEYRUNTIME_BACKEND_OPENCL_SRC = 4,
  NEXKEYRUNTIME_BACKEND_LUT_F32    = 10
} NexKeyRuntimeBackendId;

typedef struct NexKeyRuntimeAsset NexKeyRuntimeAsset;

/* Retrieves the 32-byte content key associated with `product_id` for an active license handle. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_license_get_content_key(
  const NexKeyRuntimeLicenseHandle *handle,
  const char *product_id,
  uint8_t out_cek[32]
);

/* Opens an encrypted asset container and inspects its metadata. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_asset_open(
  const char *path_utf8, 
  NexKeyRuntimeAsset **out_asset
);

NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_asset_get_manifest(
  const NexKeyRuntimeAsset *asset, 
  char *buffer, 
  size_t capacity, 
  size_t *bytes_needed
);

NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_asset_get_product_data(
  const NexKeyRuntimeAsset *asset, 
  char *buffer, 
  size_t capacity, 
  size_t *bytes_needed
);

/* Verifies and decrypts the requested backend payload into volatile memory using `cek`. */
NEXKEYRUNTIME_API NexKeyRuntimeResult nexkeyruntime_asset_decrypt(
  const uint8_t cek[32],
  NexKeyRuntimeAsset *asset,
  uint8_t backend_id,
  const uint8_t **out_plaintext,
  size_t *out_size
);

/* Securely clears decrypted plaintext buffers owned by `asset`. */
NEXKEYRUNTIME_API void nexkeyruntime_asset_release_plaintext(
  NexKeyRuntimeAsset *asset
);

/* Closes the asset handle and frees associated resources. */
NEXKEYRUNTIME_API void nexkeyruntime_asset_close(
  NexKeyRuntimeAsset *asset
);

/* Securely zeroes a caller-provided memory buffer. */
NEXKEYRUNTIME_API void nexkeyruntime_asset_test_wipe(
  uint8_t *buffer,
  size_t size
);

#ifdef __cplusplus
}
#endif

#endif
