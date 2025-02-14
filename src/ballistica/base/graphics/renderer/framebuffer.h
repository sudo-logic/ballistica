// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_BASE_GRAPHICS_RENDERER_FRAMEBUFFER_H_
#define BALLISTICA_BASE_GRAPHICS_RENDERER_FRAMEBUFFER_H_

#include "ballistica/shared/foundation/object.h"

namespace ballistica::base {

class Framebuffer : public Object {
 public:
  auto GetDefaultOwnerThread() const -> EventLoopID override {
    return EventLoopID::kMain;
  }
};

}  // namespace ballistica::base

#endif  // BALLISTICA_BASE_GRAPHICS_RENDERER_FRAMEBUFFER_H_
