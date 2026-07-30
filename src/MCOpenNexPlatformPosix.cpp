#include "MCOpenNexPlatform.h"

#include <spawn.h>
#include <string>

extern char **environ;

namespace {

bool isSupportedUrl(const std::string &url) {
  return url.find("https://") == 0 || url.find("mcnexus://") == 0;
}

} // namespace

namespace mcopen {

bool canOpenUrl(const char *value) {
  return value && isSupportedUrl(value);
}

bool openUrl(const char *value) {
  if (!canOpenUrl(value)) {
    return false;
  }
  char executable[] = "xdg-open";
  char *arguments[] = {executable, const_cast<char *>(value), nullptr};
  pid_t process = 0;
  return posix_spawnp(&process, executable, nullptr, nullptr, arguments,
                      environ) == 0;
}

bool openMCNexusApplication() { return false; }

} // namespace mcopen
