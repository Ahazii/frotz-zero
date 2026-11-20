// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform.hpp"
namespace fz {
// Shared offline URL/QR menu, also reachable from the credits.
void online_links(Display&);
// Returns the identity of a downloaded/selected story, or empty when leaving.
std::string online_library(Display&,const Settings&,const Paths&,const fs::path& fixture={});
}
