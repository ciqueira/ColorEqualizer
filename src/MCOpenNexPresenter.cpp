#include "MCOpenNexPresenter.h"

#include "MCOpenNexPlatform.h"

#include <memory>
#include <mutex>

#ifndef MCOPENNEX_BASE_URL
#define MCOPENNEX_BASE_URL "https://sdk.mcnexus.app"
#endif

namespace {

const char *currentPlatform() {
#if defined(__APPLE__)
  return "macos";
#elif defined(_WIN32)
  return "windows";
#else
  return "linux";
#endif
}

const char *currentArchitecture() {
#if defined(__aarch64__) || defined(__arm64__)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#else
  return "unknown";
#endif
}

std::string normalizedHostName(const char *hostName) {
  const std::string value = hostName ? hostName : "";
  if (value.find("DaVinci") != std::string::npos ||
      value.find("Resolve") != std::string::npos) {
    return "DaVinciResolve";
  }
  return value;
}

std::string statusForSnapshot(const MCOpenNexUpdateSnapshot &snapshot) {
  switch (snapshot.status) {
  case MCOPENNEX_UPDATE_CHECKING:
    return "Checking for Color Equalizer updates...";
  case MCOPENNEX_UPDATE_UP_TO_DATE:
    return "Color Equalizer is up to date.";
  case MCOPENNEX_UPDATE_AVAILABLE:
    return std::string("Color Equalizer ") + snapshot.latest_version +
           " is available. Open MCNexus to review the update.";
  case MCOPENNEX_UPDATE_OFFLINE:
    return "Update check unavailable while offline.";
  case MCOPENNEX_UPDATE_ERROR:
    return "Could not check for updates. Try again later.";
  case MCOPENNEX_UPDATE_IDLE:
  default:
    return "Update check has not run yet.";
  }
}

} // namespace

struct MCOpenNexRuntime {
  explicit MCOpenNexRuntime(MCOpenNexHandle *value) : handle(value) {}
  ~MCOpenNexRuntime() { mcopennex_destroy(handle); }

  MCOpenNexHandle *handle;
};

namespace {

std::mutex &sharedRuntimeMutex() {
  static std::mutex value;
  return value;
}

std::weak_ptr<MCOpenNexRuntime> &sharedRuntime() {
  static std::weak_ptr<MCOpenNexRuntime> value;
  return value;
}

} // namespace

MCOpenNexPresenter::MCOpenNexPresenter(const char *currentVersion,
                                       const char *hostName,
                                       const char *hostVersion) {
  std::lock_guard<std::mutex> lock(sharedRuntimeMutex());
  runtime_ = sharedRuntime().lock();
  if (runtime_) {
    return;
  }

  const std::string normalizedHost = normalizedHostName(hostName);
  MCOpenNexConfig config;
  mcopennex_config_init(&config);
  config.tenant_id = "colorequalizer-oss";
  config.artifact_id = "default";
  config.current_version = currentVersion;
  config.channel = "stable";
  config.platform = currentPlatform();
  config.architecture = currentArchitecture();
  config.locale = "en";
  config.host_name = normalizedHost.c_str();
  config.host_version = hostVersion;
  config.base_url = MCOPENNEX_BASE_URL;
  config.request_timeout_ms = 5000;
  config.can_open_url = canOpenUrl;
  config.open_url = openUrl;
  MCOpenNexHandle *handle = mcopennex_create(&config);
  if (handle) {
    runtime_ = std::make_shared<MCOpenNexRuntime>(handle);
    sharedRuntime() = runtime_;
  }
}

MCOpenNexPresenter::~MCOpenNexPresenter() = default;

bool MCOpenNexPresenter::isAvailable() const {
  return runtime_ && runtime_->handle;
}

void MCOpenNexPresenter::requestCheck(bool force) {
  if (isAvailable()) {
    mcopennex_request_check(runtime_->handle, force ? 1 : 0);
  }
}

MCOpenNexPresenter::ViewState MCOpenNexPresenter::viewState() const {
  ViewState result;
  if (!isAvailable()) {
    result.status = "Update service is unavailable.";
    return result;
  }

  MCOpenNexUpdateSnapshot snapshot{};
  snapshot.struct_size = sizeof(snapshot);
  if (mcopennex_get_snapshot(runtime_->handle, &snapshot) != MCOPENNEX_OK) {
    result.status = "Could not read update status.";
    return result;
  }
  result.status = statusForSnapshot(snapshot);
  result.hasAction = snapshot.has_update != 0 &&
                     (snapshot.deep_link[0] != '\0' ||
                      snapshot.fallback_url[0] != '\0');

  MCOpenNexNotice notice{};
  notice.struct_size = sizeof(notice);
  if (mcopennex_get_active_notice(runtime_->handle, &notice) == MCOPENNEX_OK &&
      notice.available != 0) {
    if (!result.status.empty()) {
      result.status += "\n";
    }
    result.status += notice.title;
    if (notice.message[0] != '\0') {
      result.status += ": ";
      result.status += notice.message;
    }
    result.hasAction =
        result.hasAction || notice.deep_link[0] != '\0' ||
        notice.fallback_url[0] != '\0';
  }
  return result;
}

bool MCOpenNexPresenter::openPrimaryAction() {
  if (!isAvailable()) {
    return false;
  }

  MCOpenNexUpdateSnapshot snapshot{};
  snapshot.struct_size = sizeof(snapshot);
  if (mcopennex_get_snapshot(runtime_->handle, &snapshot) == MCOPENNEX_OK &&
      snapshot.has_update != 0) {
    if (snapshot.deep_link[0] != '\0' &&
        mcopen::canOpenUrl(snapshot.deep_link)) {
      return mcopennex_open_update_action(runtime_->handle) == MCOPENNEX_OK;
    }
    if (mcopen::openMCNexusApplication()) {
      return true;
    }
    return mcopennex_open_update_action(runtime_->handle) == MCOPENNEX_OK;
  }

  MCOpenNexNotice notice{};
  notice.struct_size = sizeof(notice);
  if (mcopennex_get_active_notice(runtime_->handle, &notice) !=
          MCOPENNEX_OK ||
      notice.available == 0) {
    return false;
  }
  if (notice.deep_link[0] != '\0' &&
      mcopen::canOpenUrl(notice.deep_link)) {
    return mcopennex_open_notice_action(runtime_->handle, notice.id) ==
           MCOPENNEX_OK;
  }
  if (mcopen::openMCNexusApplication()) {
    return true;
  }
  return mcopennex_open_notice_action(runtime_->handle, notice.id) ==
         MCOPENNEX_OK;
}

int MCOpenNexPresenter::canOpenUrl(void *, const char *url) {
  return mcopen::canOpenUrl(url) ? 1 : 0;
}

int MCOpenNexPresenter::openUrl(void *, const char *url) {
  return mcopen::openUrl(url) ? 1 : 0;
}
