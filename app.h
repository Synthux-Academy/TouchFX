#pragma once

#include "nocopy.h"

namespace synthux {
namespace touchfx {

class Application {
  public:
    Application()  = default;
    ~Application() = default;

    void init();
    void loop();

  private:
    NOCOPY(Application)
};
};
};
