#include <daisy_seed.h>
#include "app.h"

const char* USBD_MANUFACTURER_STRING = "Synthux";
const char* USBD_PRODUCT_STRING_HS = "TouchFX";

using namespace synthux::touchfx;
static Application app;

extern "C" int main(void)
{
    app.init();
    app.loop();
}
