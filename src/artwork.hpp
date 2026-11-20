// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform.hpp"
namespace fz {
enum class Glyph { Continue, Library, NewGame, LoadSave, Settings, Refresh, Help, Exit };
// Pre-rendered at native sizes, decoded once and shared by every screen.
void brand(Display&,int x,int y,int size);
void glyph(Display&,Glyph,int x,int y,uint32_t color);
void page_header(Display&,const std::string& title,const std::string& trailing={});
void story_art(Display&,const Story&,int x,int y,int size);
void clear_story_art();
}
