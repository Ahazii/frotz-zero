// SPDX-License-Identifier: GPL-2.0-or-later
#include "app.hpp"
#include "engine_api.h"
#include "view.hpp"
#include <SDL_mixer.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <deque>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace fz {
struct Session {
    Story story;Settings settings;Paths paths;RunOptions options;
    int story_text_size=12;std::string story_theme="dark";
    std::unique_ptr<Display> display;
    fs::path folder,working,pending_save,save_target,start_generation,portable;
    std::vector<uint32_t> pixels;int width=320,height=170,pan_x=0,pan_y=0,pointer_x=160,pointer_y=85;bool pointer=false;
    double zoom=1.0;
    ViewMode view_mode=ViewMode::Automatic;
    bool reader_input=false,reader_char=false,auto_text=false,auto_art=false;
    int reader_top=0,reader_font=0;
    size_t reader_seen=0;
    uint64_t text_revision=0,reader_revision=~uint64_t(0),layout_text_revision=0;
    int input_timeout=0;
    std::vector<std::string> reader_lines;
    std::vector<size_t> reader_offsets;
    std::string upper_line;int upper_row=-1,upper_window=-1;
    int focus_x=0,focus_y=0;bool focus_valid=false,follow_cursor=true;
    uint64_t view_hint_until=0;
    std::u16string draft;size_t draft_pos=0;std::deque<std::u16string> history;std::string transcript;
    bool safe=false,restored=false,in_line=false,quitting=false,audio_silent=false;int scroll=-1,lock_fd=-1;uint64_t ticks=0;
    std::deque<uint16_t> text_queue;
    std::map<std::string,std::vector<uint8_t>> boundary;
};
static std::unique_ptr<Session> session;
static Session& S(){return *session;}
static std::string utf8(const std::u16string&s){std::string o;for(uint16_t c:s){if(c<128)o+=char(c);else if(c<2048){o+=char(0xc0|(c>>6));o+=char(0x80|(c&63));}else{o+=char(0xe0|(c>>12));o+=char(0x80|((c>>6)&63));o+=char(0x80|(c&63));}}return o;}
static std::u16string utf16(const std::string&s){std::u16string o;for(size_t i=0;i<s.size();){unsigned c=static_cast<unsigned char>(s[i++]);if(c<128)o+=char16_t(c);else{int n=c<224?1:c<240?2:3;unsigned v=c&((1<<(6-n))-1);bool ok=n<3&&i+n<=s.size();for(int j=0;j<n&&i<s.size();j++){unsigned d=static_cast<unsigned char>(s[i++]);ok&=(d&0xc0)==0x80;v=(v<<6)|(d&63);}if(ok&&v<=65535)o+=char16_t(v);}}return o;}
static std::string frontend(){auto&s=S();std::ostringstream o;o<<std::setprecision(17)<<"FZFRONT-8\n"<<s.story.id<<'\n'<<s.width<<' '<<s.height<<' '<<s.story_text_size<<' '<<s.draft_pos<<'\n'<<std::quoted(s.story_theme)<<' '<<s.zoom<<' '<<s.pan_x<<' '<<s.pan_y<<' '<<s.settings.text_size<<' '<<s.follow_cursor<<' '<<int(s.view_mode)<<' '<<s.reader_top<<' '<<s.reader_seen<<' '<<s.auto_art<<'\n'<<json_string(utf8(s.draft))<<'\n';
    // Quoted UTF-8 strings use std::quoted on reads; line breaks are escaped separately.
    o<<s.history.size()<<'\n';for(auto&h:s.history)o<<json_string(utf8(h))<<'\n';o<<s.transcript.size()<<'\n'<<s.transcript;return o.str();}
static void read_frontend(const fs::path& p) {
    auto& s=S();std::istringstream in(read_text(p,512*1024));
    std::string magic,id,draft;std::getline(in,magic);std::getline(in,id);
    bool has_reader=magic=="FZFRONT-5"||magic=="FZFRONT-6"||magic=="FZFRONT-7"||magic=="FZFRONT-8";
    if((magic!="FZFRONT-2"&&magic!="FZFRONT-3"&&magic!="FZFRONT-4"&&!has_reader)||id!=s.story.id)
        throw std::runtime_error("Checkpoint belongs to a different story or frontend version");
    size_t nh=0,nt=0;int saved_preference=s.settings.text_size;
    in>>s.width>>s.height>>s.story_text_size>>s.draft_pos;
    if(magic!="FZFRONT-2")in>>std::quoted(s.story_theme)>>s.zoom>>s.pan_x>>s.pan_y;
    if(magic=="FZFRONT-4"||has_reader)in>>saved_preference>>s.follow_cursor;
    if(has_reader){int mode=-1;in>>mode>>s.reader_top>>s.reader_seen;if(mode<0||mode>((magic=="FZFRONT-7"||magic=="FZFRONT-8")?7:magic=="FZFRONT-6"?6:5)||s.reader_top<0||s.reader_top>131072||s.reader_seen>131072)throw std::runtime_error("Invalid saved display mode");s.view_mode=static_cast<ViewMode>(mode);}
    else s.view_mode=ViewMode::Zoom;
    if(magic=="FZFRONT-8"){int original=-1;in>>original;if(original<0||original>1)throw std::runtime_error("Invalid saved original-layout state");s.auto_art=original;}
    in>>std::quoted(draft);s.draft=utf16(draft);in>>nh;
    if(nh>100||s.width<160||s.width>1280||s.height<100||s.height>800||
       s.story_text_size<12||s.story_text_size>24||!std::isfinite(s.zoom)||s.zoom<0.15||s.zoom>4||
       s.pan_x<0||s.pan_x>s.width||s.pan_y<0||s.pan_y>s.height||
       saved_preference<12||saved_preference>24||(s.story_theme!="dark"&&s.story_theme!="light"))
        throw std::runtime_error("Invalid checkpoint dimensions");
    if(magic!="FZFRONT-4"&&!has_reader){
        // Earlier schemas have no saved reading preference. Migrate to the
        // current preference instead of restoring their fit-relative shrink.
        s.zoom=double(s.settings.text_size)/s.story_text_size;s.follow_cursor=true;
    }else if(saved_preference!=s.settings.text_size){
        // Honour an explicitly changed reading preference without altering the VM's layout.
        s.zoom=double(s.settings.text_size)/s.story_text_size;s.follow_cursor=true;
    }
    s.history.clear();while(nh--){std::string v;in>>std::quoted(v);s.history.push_back(utf16(v));}
    in>>nt;in.get();if(nt>131072)throw std::runtime_error("Oversize transcript");
    s.transcript.resize(nt);in.read(s.transcript.data(),nt);
    if(!in)throw std::runtime_error("Truncated frontend state");
    s.draft_pos=std::min(s.draft_pos,s.draft.size());
    if(!has_reader)s.reader_seen=s.transcript.size();
    if(s.reader_seen>s.transcript.size())throw std::runtime_error("Invalid saved reading position");
}
static void checkpoint(const fs::path&base){auto&s=S();if(!s.safe||s.boundary.empty())throw std::runtime_error("Continue is unavailable at this input mode. Use the story's SAVE command for a portable save.");auto parts=s.boundary;auto text=frontend();parts["frontend.txt"]={text.begin(),text.end()};commit_generation(base,s.story.id,parts);}
static bool frame_window(SDL_Rect&window){auto&s=S();return s.view_mode==ViewMode::Framed&&s.in_line&&s.reader_input&&!s.pointer&&fz_main_window(&window.x,&window.y,&window.w,&window.h)&&window.x+window.w<=s.width&&window.y+window.h<=s.height;}
static Viewport current_view(){
    auto& s=S();SDL_Rect window{};bool framed=frame_window(window);
    auto viewport=[&](){return framed?framed_viewport(s.width,s.height,window,s.zoom,s.pan_x,s.pan_y):make_viewport(s.width,s.height,s.view_mode,s.zoom,s.pan_x,s.pan_y);};
    auto v=viewport();
    if(s.follow_cursor&&(s.focus_valid||s.pointer)&&(framed||s.view_mode==ViewMode::Zoom||s.view_mode==ViewMode::Pixels||s.view_mode==ViewMode::Width)){
        int x=s.pointer?s.pointer_x:s.focus_x,y=s.pointer?s.pointer_y:s.focus_y;
        double scale=double(v.target.w)/v.source.w;int margin=std::max(2,int(8/scale)),line=s.pointer?5:s.story_text_size+5;
        if(x<s.pan_x+margin)s.pan_x=x-margin;
        if(x>s.pan_x+v.source.w-margin)s.pan_x=x-v.source.w+margin;
        if(y<s.pan_y+margin)s.pan_y=y-margin;
        if(y+line>s.pan_y+v.source.h-margin)s.pan_y=y+line-v.source.h+margin;
        v=viewport();
    }
    s.pan_x=v.source.x;s.pan_y=v.source.y;return v;
}
static int reader_page_size(){return std::max(3,(134-S().settings.text_size)/(S().settings.text_size+2));}
static const std::vector<std::string>& reader_rows(){
    auto&s=S();if(s.reader_revision!=s.text_revision||s.reader_font!=s.settings.text_size){s.reader_lines=s.display->wrap(s.transcript,302,s.settings.text_size,&s.reader_offsets);if(!s.reader_lines.empty()&&(s.reader_lines.back()==">"||s.reader_lines.back()=="> ")){s.reader_lines.pop_back();s.reader_offsets.pop_back();}s.reader_revision=s.text_revision;s.reader_font=s.settings.text_size;}return s.reader_lines;
}
static int reader_limit(){return std::max(0,int(reader_rows().size())-reader_page_size());}
static bool reader_active(){auto&s=S();return s.reader_input&&!s.pointer&&(s.view_mode==ViewMode::Reader||(s.view_mode==ViewMode::Automatic&&s.auto_text&&!s.auto_art));}
static bool reader_paging(){return reader_active()&&!S().input_timeout&&S().reader_top<reader_limit();}
static void flush_upper(){
    auto&s=S();auto first=s.upper_line.find_first_not_of(" \t\r\n"),last=s.upper_line.find_last_not_of(" \t\r\n");
    if(first!=std::string::npos){if(!s.transcript.empty()&&s.transcript.back()!='\n')fz_transcript_char(10);for(auto c:utf16(s.upper_line.substr(first,last-first+1)))fz_transcript_char(c);fz_transcript_char(10);}
    s.upper_line.clear();s.upper_row=-1;s.upper_window=-1;
}
static void prepare_reader(bool restored=false){
    auto&s=S();if(s.view_mode!=ViewMode::Reader&&!(s.view_mode==ViewMode::Automatic&&s.auto_text))return;
    if(!restored&&s.transcript.size()>s.reader_seen){auto before=s.display->wrap(s.transcript.substr(0,s.reader_seen),302,s.settings.text_size);s.reader_top=std::max(0,int(before.size())-(s.reader_seen&&s.transcript[s.reader_seen-1]!='\n'?1:0));}
    if(!restored){const auto&rows=reader_rows();while(s.reader_top<reader_limit()&&rows[s.reader_top].empty())++s.reader_top;}
    s.reader_top=std::clamp(s.reader_top,0,reader_limit());s.reader_seen=s.transcript.size();
}
static void show_reader(){
    auto&s=S();auto&d=*s.display;const auto&rows=reader_rows();int page=reader_page_size(),size=s.settings.text_size;
    s.reader_top=std::clamp(s.reader_top,0,reader_limit());d.clear();d.text(8,3,s.story.title,d.muted,10,232);d.text(249,3,s.view_mode==ViewMode::Automatic?"Tab: view":"READER",d.accent,10,64);d.rect(8,17,304,1,d.panel);
    for(int i=0;i<page&&s.reader_top+i<int(rows.size());i++)d.text(8,21+i*(size+2),rows[s.reader_top+i],d.fg,size,302);
    if(reader_limit()){d.rect(315,21,2,111,d.panel);int h=std::max(5,111*page/std::max(1,int(rows.size())));d.rect(315,21+(111-h)*s.reader_top/reader_limit(),2,h,d.accent);}
    if(s.reader_char&&s.input_timeout)d.text(8,152,"PgUp/Dn read   Story keys continue",d.accent,10,304);
    else if(reader_paging())d.text(8,152,s.view_mode==ViewMode::Automatic?"Space/Enter next   Tab original view":"Space/Enter next   Arrows read   Ctrl+V view",d.accent,10,304);
    else if(s.reader_char)d.text(8,152,s.view_mode==ViewMode::Automatic?"Story keys continue   Tab original view":"Enter/Space continue   Ctrl+U/D text size",d.accent,10,304);
    else{d.rect(8,140-size/2,304,1,d.panel);auto draft="> "+utf8(s.draft),prefix="> "+utf8(s.draft.substr(0,s.draft_pos));int caret=d.text_width(prefix,size),offset=std::max(0,caret-289);d.text(8,146-size/2,draft,d.fg,size,302,offset);d.rect(8+caret-offset,148-size/2,1,size,d.accent);}
}
static void show_game(){auto&s=S();auto&d=*s.display;if(s.pixels.empty())return;
    // Fractional scales leave a narrow border; keep it with the saved story colours.
    d.rect(0,0,320,170,s.story_theme=="dark"?0xff11171f:0xfff5f3ec);
    auto surf=SDL_CreateRGBSurfaceFrom(s.pixels.data(),s.width,s.height,32,s.width*4,0xff,0xff00,0xff0000,0);
    if(s.view_mode==ViewMode::Framed){SDL_Rect full{0,0,320,170};SDL_BlitScaled(surf,nullptr,d.canvas,&full);}
    auto view=current_view();auto src=view.source,dst=view.target;
    SDL_BlitScaled(surf,&src,d.canvas,&dst);SDL_FreeSurface(surf);
    if(reader_active())show_reader();
    else if(s.view_mode==ViewMode::Automatic&&s.auto_art&&s.auto_text&&!s.pointer){d.rect(0,154,320,16,d.bg);d.text(8,156,"Tab / Space: return to text",d.accent,10,304);}
    if(s.pointer){auto [px,py]=view.screen_point(s.pointer_x,s.pointer_y);d.rect(px-4,py,9,1,d.accent);d.rect(px,py-4,1,9,d.accent);d.rect(0,154,320,16,d.bg);d.text(4,154,"Pointer: arrows move, Enter clicks",d.accent);}
    if(s.scroll>=0){d.clear();d.text(9,4,"Scrollback",d.accent,17);auto lines=d.wrap(s.transcript,302);int top=std::clamp(s.scroll,0,std::max(0,int(lines.size())-7));s.scroll=top;for(int i=0;i<7&&top+i<int(lines.size());i++)d.text(8,32+i*16,lines[top+i],d.fg,12,302);d.text(8,151,"Arrows / pages   Home start   End/Esc live",d.muted,10);}
    if(s.view_hint_until>SDL_GetTicks64()){
        d.rounded_rect(5,4,310,20,4,d.panel);
        d.text(11,8,std::string(view_name(s.view_mode))+"   "+std::to_string(s.settings.text_size)+" px   Ctrl+V: view",d.accent,10,298);
    }
    d.present();
}
static void save_view(){auto&s=S();try{atomic_text(s.folder/"display.conf","mode="+std::to_string(int(s.view_mode))+"\nautomatic_version=2\n");}catch(const std::exception&e){message(*s.display,"Display mode not saved",e.what());}}
static void set_view(ViewMode mode){auto&s=S();s.view_mode=mode;s.pan_x=s.pan_y=0;s.follow_cursor=true;s.scroll=-1;s.auto_art=false;if(mode==ViewMode::Zoom||mode==ViewMode::Framed)s.zoom=double(s.settings.text_size)/s.story_text_size;if(mode==ViewMode::Reader||mode==ViewMode::Automatic){s.reader_top=std::clamp(s.reader_top,0,reader_limit());s.pointer=false;}save_view();s.view_hint_until=SDL_GetTicks64()+1200;show_game();}
static void display_menu(){
    auto&s=S();int sel=int(s.view_mode);
    for(;;){sel=menu(*s.display,"Display mode",{"Reader: wrapped text","Fit screen: whole layout","Fill screen: stretch","Fit width: vertical pan","Actual pixels: 1 to 1","Reading zoom","Frame + text zoom","Automatic: readable text","Back"},"Saved for this story   Left / Esc back",sel);if(sel<0||sel==8)return;if(sel==0&&s.story.version==6){message(*s.display,"Graphics and text","Choose Automatic to fit artwork and wrap text at reading and command pauses. Tab switches between readable text and the original layout. Ctrl+U/D changes text size.");continue;}if(sel==6&&s.story.version!=6){message(*s.display,"For V6 graphics games","Automatic provides readable text for every supported story type. Frame + text zoom preserves a V6 game's original border and window geometry.");continue;}set_view(static_cast<ViewMode>(sel));return;}
}
static void reading_size(int size){
    auto& s=S();reader_rows();size_t anchor=s.reader_offsets.empty()?0:s.reader_offsets[std::clamp(s.reader_top,0,int(s.reader_offsets.size())-1)];s.settings.text_size=std::clamp(size,12,24);
    s.zoom=double(s.settings.text_size)/s.story_text_size;s.follow_cursor=true;
    if(s.story.version==6&&(s.view_mode==ViewMode::Fit||s.view_mode==ViewMode::Fill))s.view_mode=ViewMode::Framed;
    else if(s.view_mode!=ViewMode::Reader&&s.view_mode!=ViewMode::Framed&&s.view_mode!=ViewMode::Automatic)s.view_mode=ViewMode::Zoom;
    reader_rows();s.reader_top=std::clamp(int(std::upper_bound(s.reader_offsets.begin(),s.reader_offsets.end(),anchor)-s.reader_offsets.begin())-1,0,reader_limit());save_view();
    s.view_hint_until=SDL_GetTicks64()+1200;
    try{write_settings(s.paths,s.settings);}catch(const std::exception& e){message(*s.display,"Reading size not saved",e.what());}
    show_game();
}
static void input_focus(int x,int y){auto& s=S();s.focus_x=std::clamp(x,0,s.width-1);s.focus_y=std::clamp(y,0,s.height-1);s.focus_valid=true;}

static bool game_menu(){auto&s=S();auto&d=*s.display;fz_suspend_audio(1);std::vector<std::string> rows={"Return to story",s.safe?"Save checkpoint":"Save checkpoint: unavailable","Load save","Settings","Display: "+std::string(view_name(s.view_mode)),std::string("Mute: ")+(s.settings.mute?"on":"off"),"Help / controls","Return to library","Exit to launcher"};int choice=menu(d,"Story menu",rows);
    if(choice==1){if(!s.safe)message(d,"Portable saves remain available","Automatic resume is currently limited to ordinary untimed v3-v5/v7/v8 line prompts. V6, character input, timed prompts and active transcript/replay streams need the story's SAVE command.");else{auto name=safe_name(edit(d,"Checkpoint name","",48));if(!name.empty()){auto dest=s.folder/"saves"/(name+".fzsave");if(!fs::exists(dest)||menu(d,"Replace checkpoint?",{"Cancel","Replace "+name})==1){try{checkpoint(dest);message(d,"Checkpoint saved",name+". This includes the screen and draft; use the story's SAVE command for portable Quetzal.");}catch(const std::exception&e){message(d,"Save failed",e.what());}}}}}
    if(choice==2){auto p=choose_save(d,s.folder/"saves",s.story);if(!p.empty()){atomic_text(s.folder/"launch-request",p.string());fz_engine_exit(77);}}
    if(choice==3){int previous=s.settings.text_size;settings_menu(d,s.settings,s.paths,true);if(previous!=s.settings.text_size){int selected=s.settings.text_size;s.settings.text_size=previous;reading_size(selected);}}
    if(choice==4)display_menu();
    if(choice==5){s.settings.mute=!s.settings.mute;write_settings(s.paths,s.settings);}
    if(choice==6)help(d);
    if(choice==7||choice==8){if(s.safe){try{checkpoint(s.folder/"resume");}catch(const std::exception&e){message(d,"Resume not written",e.what());if(menu(d,"Leave without resume?",{"Cancel","Leave anyway"})!=1){fz_suspend_audio(0);show_game();return false;}}}else if(menu(d,"No current resume",{"Cancel","Leave (keep named saves)"})!=1){fz_suspend_audio(0);show_game();return false;}fz_engine_exit(choice==8?78:0);}
    fz_suspend_audio(0);show_game();return true;
}
static uint16_t key_for(const SDL_Event&e){auto&s=S();auto&d=*s.display;
    if(e.type==SDL_QUIT){if(s.safe){try{checkpoint(s.folder/"resume");}catch(const std::exception&x){message(d,"Resume failed",x.what());return 0;}}fz_engine_exit(0);}
    if(e.type==SDL_WINDOWEVENT){show_game();return 0;}
    if(e.type==SDL_TEXTINPUT){if(s.scroll>=0)return 0;if(s.view_mode==ViewMode::Automatic&&s.auto_art&&s.auto_text){s.auto_art=false;show_game();if(std::string(e.text.text)==" ")return 0;}if(reader_paging()){if(std::string(e.text.text)==" "){s.reader_top=std::min(reader_limit(),s.reader_top+reader_page_size());show_game();return 0;}s.reader_top=reader_limit();}s.follow_cursor=true;for(auto c:utf16(e.text.text))s.text_queue.push_back(c);return 0;}
    if(e.type==SDL_MOUSEBUTTONDOWN){auto point=current_view().story_point(e.button.x,e.button.y);s.pointer_x=point.first;s.pointer_y=point.second;return e.button.clicks>1?156:155;}
    if(e.type!=SDL_KEYDOWN)return 0;auto k=e.key.keysym.sym;auto mod=e.key.keysym.mod;bool ctrl=mod&KMOD_CTRL;
    if(s.reader_char&&reader_active()&&e.key.repeat&&(k==SDLK_RETURN||k==SDLK_KP_ENTER||k==SDLK_SPACE))return 0;
    if((ctrl&&k==SDLK_ESCAPE)||k==SDLK_HELP){if(k==SDLK_HELP)help(d);else game_menu();show_game();return 0;}
    if(ctrl&&k==SDLK_v){fz_suspend_audio(1);display_menu();fz_suspend_audio(0);show_game();return 0;}
    if(k==SDLK_AUDIOMUTE){s.settings.mute=!s.settings.mute;write_settings(s.paths,s.settings);fz_suspend_audio(0);return 0;}
    if(k==SDLK_PRINTSCREEN){d.screenshot(s.paths.data/"screenshots"/("private-"+timestamp()+".png"));return 0;}
    if(s.scroll>=0){if(k==SDLK_END||k==SDLK_ESCAPE)s.scroll=-1;else if(k==SDLK_HOME)s.scroll=0;else if(k==SDLK_PAGEUP||k==SDLK_UP)s.scroll=std::max(0,s.scroll-(k==SDLK_UP?1:7));else if(k==SDLK_PAGEDOWN||k==SDLK_DOWN)s.scroll+=(k==SDLK_DOWN?1:7);show_game();return 0;}
    if(!ctrl&&k==SDLK_TAB&&s.view_mode==ViewMode::Automatic&&s.reader_input&&s.auto_text&&!s.pointer){s.auto_art=!s.auto_art;show_game();return 0;}
    if(!ctrl&&s.auto_art&&s.view_mode==ViewMode::Automatic&&s.auto_text&&(k==SDLK_RETURN||k==SDLK_KP_ENTER)){s.auto_art=false;show_game();return 0;}
    if(!ctrl&&reader_active()){
        bool arrows=!s.reader_char||s.view_mode==ViewMode::Reader;
        int step=arrows&&k==SDLK_UP?-1:arrows&&k==SDLK_DOWN?1:k==SDLK_PAGEUP?-reader_page_size():k==SDLK_PAGEDOWN?reader_page_size():0;
        if(step){s.reader_top=std::clamp(s.reader_top+step,0,reader_limit());show_game();return 0;}
        if(reader_paging()||s.reader_char){
            if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&reader_paging()){s.reader_top=std::min(reader_limit(),s.reader_top+reader_page_size());show_game();return 0;}
            if(k==SDLK_HOME){s.reader_top=0;show_game();return 0;}
            if(k==SDLK_END||(k==SDLK_ESCAPE&&(s.view_mode!=ViewMode::Automatic||!s.reader_char))){s.reader_top=reader_limit();show_game();return 0;}
        }
    }
    if(k==SDLK_PAGEUP||k==SDLK_PAGEDOWN){int n=d.wrap(s.transcript,302).size();s.scroll=std::max(0,n-7);show_game();return 0;}
    if(ctrl&&k==SDLK_SPACE){
        auto v=current_view();s.pointer=!s.pointer;
        if(s.pointer&&(s.pointer_x<v.source.x||s.pointer_x>=v.source.x+v.source.w||s.pointer_y<v.source.y||s.pointer_y>=v.source.y+v.source.h)){
            s.pointer_x=v.source.x+v.source.w/2;s.pointer_y=v.source.y+v.source.h/2;
        }
        s.follow_cursor=true;show_game();return 0;
    }
    if(ctrl&&(k==SDLK_u||k==SDLK_d||k==SDLK_PLUS||k==SDLK_EQUALS||k==SDLK_MINUS)){
        reading_size(s.settings.text_size+((k==SDLK_d||k==SDLK_MINUS)?-2:2));return 0;
    }
    if(ctrl&&k==SDLK_0){
        set_view(s.view_mode==ViewMode::Fit?ViewMode::Automatic:ViewMode::Fit);return 0;
    }
    // The Zero cannot combine Ctrl with its Fn/Sym layers. Use the letters
    // bearing the printed arrows, retaining older aliases for desktop keyboards.
    if(ctrl&&(k==SDLK_f||k==SDLK_x||k==SDLK_z||k==SDLK_c||
        (!s.pointer&&(k==SDLK_LEFT||k==SDLK_RIGHT||((mod&KMOD_SHIFT)&&(k==SDLK_UP||k==SDLK_DOWN)))))){
        if(s.view_mode==ViewMode::Reader||s.view_mode==ViewMode::Automatic||s.view_mode==ViewMode::Fit||s.view_mode==ViewMode::Fill){s.view_mode=ViewMode::Zoom;s.zoom=double(s.settings.text_size)/s.story_text_size;save_view();}
        s.follow_cursor=false;
        s.pan_x+=(k==SDLK_c||k==SDLK_RIGHT)?24:(k==SDLK_z||k==SDLK_LEFT)?-24:0;
        s.pan_y+=(k==SDLK_x||k==SDLK_DOWN)?24:(k==SDLK_f||k==SDLK_UP)?-24:0;
        show_game();return 0;
    }
    if(s.pointer){if(k==SDLK_ESCAPE)s.pointer=false;if(k==SDLK_UP)s.pointer_y=std::max(0,s.pointer_y-4);if(k==SDLK_DOWN)s.pointer_y=std::min(s.height-1,s.pointer_y+4);if(k==SDLK_LEFT)s.pointer_x=std::max(0,s.pointer_x-4);if(k==SDLK_RIGHT)s.pointer_x=std::min(s.width-1,s.pointer_x+4);show_game();if(k==SDLK_RETURN)return 155;return 0;}
    if(ctrl&&(k==SDLK_p||k==SDLK_UP)){s.follow_cursor=true;return 0x201;}
    if(ctrl&&(k==SDLK_n||k==SDLK_DOWN)){s.follow_cursor=true;return 0x202;}
    if(k==SDLK_LEFT||k==SDLK_RIGHT||k==SDLK_HOME||k==SDLK_END||k==SDLK_BACKSPACE||k==SDLK_DELETE)s.follow_cursor=true;
    switch(k){case SDLK_RETURN:case SDLK_KP_ENTER:return 13;case SDLK_BACKSPACE:return 8;case SDLK_DELETE:return 0x100;case SDLK_ESCAPE:return 27;case SDLK_HOME:return 0x203;case SDLK_END:return 0x204;case SDLK_UP:return 129;case SDLK_DOWN:return 130;case SDLK_LEFT:return 131;case SDLK_RIGHT:return 132;default:if(k>=SDLK_F1&&k<=SDLK_F12)return 133+k-SDLK_F1;return 0;}
}
static uint16_t next_key(uint64_t&deadline){auto&s=S();for(;;){if(s.view_hint_until&&SDL_GetTicks64()>=s.view_hint_until){s.view_hint_until=0;show_game();}if(deadline&&SDL_GetTicks64()>=deadline)return 0;if(!s.text_queue.empty()){auto k=s.text_queue.front();s.text_queue.pop_front();return k;}SDL_Event e{};if(s.display->poll(&e)){auto before=SDL_GetTicks64();auto k=key_for(e);if(deadline)deadline+=SDL_GetTicks64()-before;if(k){if(k==155||k==156)fz_set_mouse(s.pointer_x+1,s.pointer_y+1);return k;}}fz_check_audio();SDL_Delay(10);}}
int run_engine(const Story&story,Settings settings,const Paths&paths,const RunOptions&options){
    session=std::make_unique<Session>();auto&s=S();s.story=story;s.settings=settings;s.story_text_size=settings.text_size;s.story_theme=settings.theme;s.paths=paths;s.options=options;s.folder=paths.data/"stories"/story.id;s.working=s.folder/"working";fs::create_directories(s.working);fs::create_directories(s.folder/"saves");
    s.lock_fd=open((s.folder/"session.lock").c_str(),O_CREAT|O_RDWR,0600);if(s.lock_fd<0||flock(s.lock_fd,LOCK_EX|LOCK_NB))throw std::runtime_error("This story is already open in another Frotz Zero instance.");
    // The same release/serial recognition as the pinned Frotz core; renames do
    // not change the layout or the per-content save identity.
    bool beyond=false;for(auto identity:std::vector<std::pair<int,std::string>>{{1,"870412"},{1,"870715"},{47,"870915"},{49,"870917"},{51,"870923"},{57,"871221"},{60,"880610"}})beyond|=story.version==5&&story.release==identity.first&&story.serial==identity.second;
    s.width=story.version==4||story.version==6||beyond?640:320;s.height=s.width==640?400:170;
    // Keep the VM font stable in every new session. A larger reading preference
    // reflows the frontend rather than reducing the VM to a few short lines.
    // Checkpoints below retain their original dimensions and font metrics.
    s.story_text_size=12;
    s.zoom=double(settings.text_size)/s.story_text_size;
    // Adopt Automatic once for every version. A later explicit choice belongs
    // to the story and takes precedence over an older checkpoint's view mode.
    bool current_view_preference=false;
    ViewMode preferred_view=ViewMode::Automatic;
    try{auto pref=read_text(s.folder/"display.conf",128);if(pref.rfind("mode=",0)==0&&pref.find("\nautomatic_version=2\n")!=std::string::npos){int mode=std::stoi(pref.substr(5));if(mode>=0&&mode<=7&&!(mode==0&&story.version==6)&&!(mode==6&&story.version!=6)){preferred_view=static_cast<ViewMode>(mode);current_view_preference=true;}}}catch(...){}
    if(options.resume||options.load.extension()==".fzsave"){
        auto g=find_generation(options.load.empty()?s.folder/"resume":options.load,story.id);if(!g)throw std::runtime_error("No valid checkpoint. Source files preserved; select New Game or a named save.");s.start_generation=g.path;read_frontend(g.path/"frontend.txt");
    }else if(!options.load.empty()){if(!portable_save_matches(options.load,story))throw std::runtime_error("Save identity or structure mismatch");s.portable=options.load;}
    s.display=std::make_unique<Display>(options.device,options.scale);s.display->theme(settings.theme=="dark");if(!options.drive.empty())s.display->load_drive(options.drive);
    if(s.view_mode!=preferred_view){
        bool saved_prose=s.view_mode==ViewMode::Automatic||s.view_mode==ViewMode::Reader;
        s.view_mode=preferred_view;s.auto_art=false;s.pan_x=s.pan_y=0;s.follow_cursor=true;
        if(!s.start_generation.empty()&&!saved_prose&&preferred_view==ViewMode::Automatic)s.reader_top=reader_limit();
    }
    if(!current_view_preference)save_view();
    // SDL selects the firmware's managed PulseAudio/PipeWire output. Explicit
    // SDL audio environment overrides remain available for other device images.
    std::ostringstream cfg;cfg<<"[Fonts]\nantialias=1\n";std::vector<std::string> styles={"roman","bold","italic","bolditalic"};for(int i=0;i<4;i++)for(bool mono:{false,true})cfg<<(mono?"fixed":"text")<<styles[i]<<'='<<s.display->font_path(mono,i)<<'@'<<s.story_text_size<<'\n';cfg<<"[Window]\nAcWidth="<<s.width<<"\nAcHeight="<<s.height<<"\n[Display]\nInfocom V6 Scaling X=2\nInfocom V6 Scaling Y=2\nForeground="<<(s.story_theme=="dark"?0xf4ede7:0x37291c)<<"\nBackground="<<(s.story_theme=="dark"?0x1f1711:0xecf3f5)<<"\nShow More Prompts=1\n";
    auto conf=s.working/"engine.conf";atomic_text(conf,cfg.str());std::vector<std::string>args={"frotz-zero","-C",conf.string(),"-u","8"};if(!s.portable.empty()){args.push_back("-L");args.push_back(s.portable.string());}args.push_back(story.path.string());if(!story.resource.empty()&&story.resource!=story.path)args.push_back(story.resource.string());std::vector<char*>argv;for(auto&a:args)argv.push_back(a.data());argv.push_back(nullptr);return fz_vm_main(args.size(),argv.data());
}
} // namespace fz
using namespace fz;
extern "C" {
void fz_video_init(int w,int h){S().width=w;S().height=h;}
void fz_present_pixels(const uint32_t*p,int w,int h){auto&s=S();s.width=w;s.height=h;s.pixels.assign(p,p+size_t(w)*h);show_game();}
int fz_has_pictures(){return S().story.pictures;}
int fz_has_sound(){return S().story.sound;}
int fz_audio_volume(int volume){return S().settings.mute?0:volume*S().settings.volume/100;}
void fz_audio_unavailable(){auto&s=S();s.audio_silent=true;try{atomic_text(s.folder/"audio-status","Audio output unavailable. A silent SDL mixer preserves resource timing and completion callbacks. Check ALSA output and speaker volume.");}catch(...){}std::cerr<<"Frotz Zero: audio output unavailable; using silent timing backend.\n";}
uint16_t fz_mouse_state(int*x,int*y){*x=S().pointer_x+1;*y=S().pointer_y+1;return 0;}
void fz_capture_boundary(int safe){auto&s=S();flush_upper();s.safe=safe;s.boundary.clear();s.in_line=false;s.reader_input=false;
    if(!safe){try{atomic_text(s.folder/"resume-unavailable","Current input mode has no supported automatic resume. Use named SAVE/RESTORE.");}catch(...){}return;}
    try{fz_flush();prepare_reader(s.restored);if(!fz_vm_save((s.working/"vm.bin").c_str())||!fz_screen_save((s.working/"screen.bin").c_str()))throw std::runtime_error("Engine snapshot failed");s.boundary["vm.bin"]=read_bytes(s.working/"vm.bin");s.boundary["screen.bin"]=read_bytes(s.working/"screen.bin");if(!s.restored){s.draft.clear();s.draft_pos=0;}checkpoint(s.folder/"resume");fs::remove(s.folder/"resume-unavailable");}catch(const std::exception&e){s.safe=false;std::cerr<<"Resume unavailable: "<<e.what()<<'\n';try{atomic_text(s.folder/"last-error",e.what());atomic_text(s.folder/"resume-unavailable",e.what());}catch(...){}if(s.options.drive.empty())message(*s.display,"Resume could not be written",e.what());}}
void fz_engine_started(){auto&s=S();if(!s.start_generation.empty()){if(!fz_vm_restore((s.start_generation/"vm.bin").c_str())||!fz_screen_restore((s.start_generation/"screen.bin").c_str())){fz_engine_error("Checkpoint engine state could not be restored. No new game was started; files preserved.");fz_engine_exit(1);}s.start_generation.clear();s.restored=true;fz_flush();}}
uint16_t fz_read_line(int max,uint16_t*buf,int timeout,int width,int continued){auto&s=S();flush_upper();s.in_line=true;s.reader_char=false;s.input_timeout=timeout;s.reader_input=!timeout||s.view_mode==ViewMode::Automatic;s.auto_text=true;if(!s.restored)s.auto_art=false;prepare_reader(s.restored);if(timeout){s.safe=false;try{atomic_text(s.folder/"resume-unavailable","Timed input: use a named save; automatic continuation is not verified.");}catch(...){}}
    int x,y;fz_get_text_position(&x,&y);x-=fz_text_width(buf);std::u16string line(reinterpret_cast<char16_t*>(buf));size_t pos=continued?std::min(s.draft_pos,line.size()):line.size();if(s.restored){line=s.draft;pos=std::min(s.draft_pos,line.size());s.restored=false;}line.resize(std::min(line.size(),size_t(max)));int history_index=s.history.size();std::u16string pending=line;
    uint64_t deadline=timeout?SDL_GetTicks64()+uint64_t(timeout)*100:0;
    auto draw=[&](){std::copy(line.begin(),line.end(),buf);buf[line.size()]=0;s.draft=line;s.draft_pos=pos;auto prefix=line.substr(0,pos);
        input_focus(x+std::min(std::max(0,width-12),fz_text_width(reinterpret_cast<const uint16_t*>(prefix.c_str()))),y);
        fz_draw_input(buf,pos,x,y,width,1);show_game();};draw();
    while(true){auto key=next_key(deadline);if(!key&&deadline){draw();s.in_line=false;s.reader_input=false;return 0;}
        if(key==0x201){if(history_index==int(s.history.size()))pending=line;if(history_index>0){line=s.history[--history_index];line.resize(std::min(line.size(),size_t(max)));pos=line.size();}}
        else if(key==0x202){if(history_index<int(s.history.size())){history_index++;line=history_index==int(s.history.size())?pending:s.history[history_index];line.resize(std::min(line.size(),size_t(max)));pos=line.size();}}
        else if(key==131&&pos)pos--;else if(key==132&&pos<line.size())pos++;else if(key==0x203)pos=0;else if(key==0x204)pos=line.size();else if(key==8&&pos)line.erase(--pos,1);else if(key==0x100&&pos<line.size())line.erase(pos,1);
        else if(fz_is_terminator(key)){draw();fz_draw_input(buf,pos,x,y,width,0);if(key==13&&!line.empty()){s.history.push_back(line);if(s.history.size()>64)s.history.pop_front();for(auto c:line)fz_transcript_char(c);fz_transcript_char(10);}s.draft.clear();s.draft_pos=0;s.in_line=false;s.reader_input=false;s.reader_seen=s.transcript.size();return key;}
        else if(fz_char_valid(key)&&line.size()<size_t(max)){line.insert(line.begin()+pos,key);pos++;}draw();
    }
}
uint16_t fz_read_char(int timeout,int cursor){
    (void)cursor;auto&s=S();
    // Interpreter [More] pauses can occur during output, between VM input
    // boundaries. Never pair their new frontend text with an older VM snapshot.
    if(s.safe)fz_capture_boundary(0);
    fz_flush();flush_upper();s.in_line=false;s.reader_char=true;s.input_timeout=timeout;
    bool text=s.text_revision>s.layout_text_revision&&!s.transcript.empty();
    if(!text||!s.auto_text)s.auto_art=false;
    s.auto_text=text;s.reader_input=(!timeout||s.view_mode==ViewMode::Automatic)&&!s.transcript.empty();
    prepare_reader();int x,y;fz_get_text_position(&x,&y);input_focus(x,y);show_game();
    auto deadline=timeout?SDL_GetTicks64()+uint64_t(timeout)*100:0;
    for(;;){auto k=next_key(deadline);if(k<0x100){s.reader_seen=s.transcript.size();s.reader_input=false;s.reader_char=false;return k;}}
}
void fz_tick(){auto&s=S();auto now=SDL_GetTicks64();if(now-s.ticks<20)return;s.ticks=now;fz_check_audio();fz_flush();SDL_Event e;if(s.display->poll(&e,false)){if(e.type==SDL_QUIT){s.safe=false;fz_engine_exit(0);}else s.display->defer(e);}}
void fz_transcript_char(uint16_t c){auto&s=S();if(c==13)c=10;if(c<32&&c!=10)return;s.transcript+=utf8(std::u16string(1,c));++s.text_revision;if(s.transcript.size()>131072){size_t trim=s.transcript.size()-131072;while(trim<s.transcript.size()&&(static_cast<unsigned char>(s.transcript[trim])&0xc0)==0x80)++trim;s.transcript.erase(0,trim);s.reader_seen=s.reader_seen>trim?s.reader_seen-trim:0;}}
void fz_transcript_window(uint16_t c,int window,int row){
    auto&s=S();if(window==0){flush_upper();fz_transcript_char(c);return;}
    if(row!=s.upper_row||window!=s.upper_window||c==10||c==13)flush_upper();s.upper_row=row;s.upper_window=window;
    if(c>=32&&s.upper_line.size()<2048)s.upper_line+=utf8(std::u16string(1,c));
}
void fz_layout_changed(){auto&s=S();if(!s.start_generation.empty())return;flush_upper();s.layout_text_revision=s.text_revision;s.auto_art=false;}
void fz_transcript_erase(int n){auto&s=S();while(n-->0&&!s.transcript.empty())s.transcript.pop_back();++s.text_revision;s.reader_seen=std::min(s.reader_seen,s.transcript.size());}
char* fz_file_dialog(const char*def,int flag){auto&s=S();auto&d=*s.display;
    if(flag==0){if(!s.portable.empty()){auto p=s.portable;s.portable.clear();return strdup(p.c_str());}auto p=choose_save(d,s.folder/"saves",s.story);show_game();if(p.empty())return nullptr;if(p.extension()!=".qzl"){message(d,"Use Load save menu","Frontend checkpoints are opened through the app menu; the story's RESTORE reads portable Quetzal only.");show_game();return nullptr;}return strdup(p.c_str());}
    if(flag==1){auto name=safe_name(edit(d,"Save name","",48));if(name.empty()){show_game();return nullptr;}s.save_target=s.folder/"saves"/(name+".qzl");if(fs::exists(s.save_target)&&menu(d,"Overwrite named save?",{"Cancel","Replace "+name})!=1){show_game();return nullptr;}s.pending_save=s.working/("save-"+timestamp()+".qzl");show_game();return strdup(s.pending_save.c_str());}
    // Story-provided paths are never opened outside per-story user storage.
    auto name=safe_name(fs::path(def?def:"aux").filename().stem().string());if(name.empty())name="aux";auto dir=s.folder/"files";fs::create_directories(dir);auto target=dir/(name+(flag==2?".txt":flag==3||flag==4?".rec":".aux"));return strdup(target.c_str());
}
int fz_save_complete(int success){auto&s=S();if(s.pending_save.empty())return success;try{if(!success||!portable_save_matches(s.pending_save,s.story))throw std::runtime_error("Save write incomplete; existing save preserved");atomic_write(s.save_target,read_bytes(s.pending_save));fs::remove(s.pending_save);s.pending_save.clear();return 1;}catch(const std::exception&e){message(*s.display,"Save failed",e.what());show_game();return 0;}}
void fz_engine_error(const char*message){auto&s=S();try{atomic_text(s.folder/"last-error",message);}catch(...){}std::cerr<<"Frotz Zero: "<<message<<'\n';if(s.options.drive.empty()&&s.display)fz::message(*s.display,"Story stopped",message);}
void fz_engine_exit(int status){
    if(session){auto&s=S();
        if(!s.options.drive.empty())try{
            auto v=current_view();std::string visible;const auto&rows=reader_rows();
            for(int i=s.reader_top;i<std::min(int(rows.size()),s.reader_top+reader_page_size());++i)visible+=rows[i]+"\n";
            atomic_text(s.working/"drive-report.json","{\"status\":"+std::to_string(status)+",\"transcript\":"+json_string(s.transcript)+",\"draft\":"+json_string(utf8(s.draft))+
                ",\"safe_resume\":"+(s.safe?"true":"false")+",\"reading_size\":"+std::to_string(s.settings.text_size)+",\"scale\":"+std::to_string(s.zoom)+
                ",\"view_mode\":"+json_string(view_name(s.view_mode))+",\"reader_active\":"+(reader_active()?"true":"false")+",\"input_timeout\":"+std::to_string(s.input_timeout)+",\"input_kind\":"+json_string(s.in_line?"line":s.reader_char?"character":"other")+",\"reader_top\":"+std::to_string(s.reader_top)+",\"reader_limit\":"+std::to_string(reader_limit())+",\"reader_visible\":"+json_string(visible)+
                ",\"pan_x\":"+std::to_string(s.pan_x)+",\"pan_y\":"+std::to_string(s.pan_y)+",\"source_width\":"+std::to_string(v.source.w)+",\"source_height\":"+std::to_string(v.source.h)+
                ",\"target_width\":"+std::to_string(v.target.w)+",\"target_height\":"+std::to_string(v.target.h)+",\"canvas_width\":"+std::to_string(s.width)+",\"canvas_height\":"+std::to_string(s.height)+"}\n");
        }catch(...){}
        if(!s.options.capture.empty()&&s.display)try{s.display->screenshot(s.options.capture);}catch(...){}
        if(s.lock_fd>=0){flock(s.lock_fd,LOCK_UN);close(s.lock_fd);s.lock_fd=-1;}s.display.reset();
    }std::exit(status);
}
}
