#ifndef MI_E2EE_SERVER_FRAME_ROUTER_H
#define MI_E2EE_SERVER_FRAME_ROUTER_H

#include <string>

#include "api_service.h"
#include "frame.h"

namespace mi::server {

class FrameRouter {
 public:
  explicit FrameRouter(ApiService* api);

  //  ApiService
  bool Handle(const Frame& in, Frame& out, const std::string& token);

 private:
  ApiService* api_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_FRAME_ROUTER_H
