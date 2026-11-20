// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform.hpp"
namespace fz {
struct RunOptions {bool device=false;int scale=3;fs::path drive,capture,load;bool resume=false;};
int run_engine(const Story&,Settings,const Paths&,const RunOptions&);
fs::path choose_save(Display&,const fs::path&,const Story&);
}
