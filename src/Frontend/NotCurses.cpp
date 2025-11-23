#include "NotCurses.hpp"


Frontends::NotCursesFrontend::NotCursesFrontend()
    : nc{ notcurses_options{ .termtype = nullptr,
                             .loglevel = NCLOGLEVEL_WARNING,
                             .margin_t = 0, .margin_r = 0,
                             .margin_b = 0, .margin_l = 0,
                             .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE } }
{
}
