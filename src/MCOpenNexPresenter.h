#ifndef MC_COLOR_EQUALIZER_MCOPENNEX_PRESENTER_H
#define MC_COLOR_EQUALIZER_MCOPENNEX_PRESENTER_H

#include <mcopennex/mcopennex.h>

#include <memory>
#include <string>

struct MCOpenNexRuntime;

class MCOpenNexPresenter {
public:
  struct ViewState {
    std::string status;
    bool hasAction = false;
  };

  MCOpenNexPresenter(const char *currentVersion, const char *hostName,
                     const char *hostVersion);
  ~MCOpenNexPresenter();

  MCOpenNexPresenter(const MCOpenNexPresenter &) = delete;
  MCOpenNexPresenter &operator=(const MCOpenNexPresenter &) = delete;

  bool isAvailable() const;
  void requestCheck(bool force);
  ViewState viewState() const;
  bool openPrimaryAction();

private:
  static int canOpenUrl(void *userData, const char *url);
  static int openUrl(void *userData, const char *url);

  std::shared_ptr<MCOpenNexRuntime> runtime_;
};

#endif
