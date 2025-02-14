// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_BASE_PLATFORM_WINDOWS_BASE_PLATFORM_WINDOWS_H_
#define BALLISTICA_BASE_PLATFORM_WINDOWS_BASE_PLATFORM_WINDOWS_H_
#if BA_OSTYPE_WINDOWS

#include <string>
#include <vector>

#include "ballistica/base/platform/base_platform.h"

namespace ballistica::base {

class BasePlatformWindows : public BasePlatform {
 public:
  BasePlatformWindows();
  void DoOpenURL(const std::string& url) override;
  void SetupInterruptHandling() override;
};

}  // namespace ballistica::base

#endif  // BA_OSTYPE_WINDOWS
#endif  // BALLISTICA_BASE_PLATFORM_WINDOWS_BASE_PLATFORM_WINDOWS_H_
