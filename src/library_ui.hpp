// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform.hpp"
namespace fz {
enum class HomeAction { Continue, Library, Downloads, Settings, Help, Exit };
enum class StoryAction { Back=-1, Continue, NewGame, LoadSave, Delete };
HomeAction home_screen(Display&,const Story*,size_t count,bool resumable,int selected);
StoryAction story_screen(Display&,const Story&,bool resumable);
// -1 returns home; -2 opens downloads; -3 refreshes the library.
int story_picker(Display&,const std::vector<Story>&,int selected);
void story_details(Display&,const Story&);
}
