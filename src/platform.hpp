// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <deque>
#include <set>
#include "model.hpp"
namespace fz {
struct Key { SDL_Keycode code=0; uint16_t mods=0; std::string text; bool quit=false; };
class Display {
    SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr; SDL_Texture* texture=nullptr;
    int fb=-1;void* fbmem=nullptr;size_t fbsize=0;std::vector<int> keyboards;
    void* key_context=nullptr;void* key_map=nullptr;void* key_state=nullptr;
    std::set<int> pressed_keys;
    int pitch=0,bpp=0,xoffset=0,yoffset=0;unsigned red_offset=0,red_length=0,green_offset=0,green_length=0,blue_offset=0,blue_length=0;
    TTF_Font* font=nullptr;TTF_Font* heading=nullptr;TTF_Font* small=nullptr;
    mutable std::map<int,TTF_Font*> reading_fonts;
    TTF_Font* ui_font(int size)const;
    std::deque<std::pair<uint64_t,std::string>> drive;
    std::deque<SDL_Event> event_queue;
    uint64_t drive_start=0;bool quit_requested=false;
    bool capture_exit=false;
    void init_keyboard();
    bool keyboard_event(int code,int value,SDL_Event* event);
public:
    fs::path capture_once;
    SDL_Surface* canvas=nullptr;
    bool device=false,dark=true;
    uint32_t bg=0xff0d1b2a,fg=0xfff5f2e7,muted=0xffb0c2cb,accent=0xff74dfc4,panel=0xff182d40,gold=0xffe9bd75;
    Display(bool device,int scale=3);
    ~Display();
    void theme(bool is_dark);
    void clear();
    void rect(int x,int y,int w,int h,uint32_t c);
    void rounded_rect(int x,int y,int w,int h,int radius,uint32_t c);
    void text(int x,int y,const std::string&,uint32_t color=0,int size=12,int max_width=310,int offset=0);
    int text_width(const std::string&,int size=12)const;
    std::vector<std::string> wrap(const std::string&,int width,int size=12,std::vector<size_t>* offsets=nullptr)const;
    void paragraph(int x,int y,const std::string&,int width=300,int lines=7);
    void present();
    void screenshot(const fs::path&);
    bool poll(SDL_Event*,bool include_deferred=true);
    void defer(const SDL_Event&e){if(event_queue.size()<256)event_queue.push_back(e);}
    Key wait(int timeout=100);
    void load_drive(const fs::path&);
    std::string font_path(bool mono=false,int style=0)const;
};
int menu(Display&,const std::string&title,const std::vector<std::string>&rows,const std::string&hint="Enter select   Esc back",int selected=0,int text_size=12);
void message(Display&,const std::string&title,const std::string&body);
std::string edit(Display&,const std::string&title,const std::string&initial,size_t limit=240);
void settings_menu(Display&,Settings&,const Paths&,bool playing=false);
void help(Display&);
// Shared menu routing. Plain F/X/Z/C work only outside editable text fields.
bool menu_back(const Key&);
bool menu_accept(const Key&);
int menu_step(const Key&);
int menu_digit(const Key&);
}
