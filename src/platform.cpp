// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform.hpp"
#include "artwork.hpp"
#include <png.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
namespace fz {
static SDL_Keycode evkey(int c){switch(c){case KEY_ESC:return SDLK_ESCAPE;case KEY_ENTER:return SDLK_RETURN;case KEY_UP:return SDLK_UP;case KEY_DOWN:return SDLK_DOWN;case KEY_LEFT:return SDLK_LEFT;case KEY_RIGHT:return SDLK_RIGHT;case KEY_BACKSPACE:return SDLK_BACKSPACE;case KEY_DELETE:return SDLK_DELETE;case KEY_HOME:return SDLK_HOME;case KEY_END:return SDLK_END;case KEY_PAGEUP:return SDLK_PAGEUP;case KEY_PAGEDOWN:return SDLK_PAGEDOWN;case KEY_TAB:return SDLK_TAB;case KEY_HELP:return SDLK_HELP;case KEY_SYSRQ:return SDLK_PRINTSCREEN;case KEY_MUTE:return SDLK_AUDIOMUTE;default:if(c>=KEY_F1&&c<=KEY_F10)return SDLK_F1+c-KEY_F1;if(c==KEY_F11)return SDLK_F11;if(c==KEY_F12)return SDLK_F12;return 0;}}
std::string Display::font_path(bool mono,int style)const{
    if(style){auto stem=mono?"DejaVuSansMono":"DejaVuSans";auto suffix=style==1?"-Bold.ttf":style==2?"-Oblique.ttf":"-BoldOblique.ttf";auto p=fs::path("/usr/share/fonts/truetype/dejavu")/(std::string(stem)+suffix);if(fs::is_regular_file(p))return p.string();}
    const char* override=getenv(mono?"FROTZ_ZERO_MONO_FONT":"FROTZ_ZERO_UI_FONT");if(override&&fs::is_regular_file(override))return override;
    for(auto p:mono?std::vector<std::string>{"/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"}:std::vector<std::string>{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"})if(fs::is_regular_file(p))return p;
    throw std::runtime_error("DejaVu fonts missing. Install fonts-dejavu-core.");
}
Display::Display(bool dev,int scale):device(dev){
    if(SDL_Init((dev?0:SDL_INIT_VIDEO)|SDL_INIT_TIMER|SDL_INIT_EVENTS)!=0)throw std::runtime_error(SDL_GetError());
    if(TTF_Init()!=0)throw std::runtime_error(TTF_GetError());
    font=TTF_OpenFont(font_path().c_str(),12);heading=TTF_OpenFont(font_path().c_str(),17);small=TTF_OpenFont(font_path().c_str(),10);
    if(!font||!heading||!small)throw std::runtime_error("Cannot load interface fonts");
    canvas=SDL_CreateRGBSurfaceWithFormat(0,320,170,32,SDL_PIXELFORMAT_ARGB8888);if(!canvas)throw std::runtime_error(SDL_GetError());
    if(dev){
        auto name=getenv("FROTZ_ZERO_FB");fb=open(name?name:"/dev/fb0",O_RDWR|O_CLOEXEC);if(fb<0)throw std::runtime_error("Cannot open framebuffer as this user. Check the device report; no root launcher is used.");
        fb_var_screeninfo v{};fb_fix_screeninfo f{};if(ioctl(fb,FBIOGET_VSCREENINFO,&v)||ioctl(fb,FBIOGET_FSCREENINFO,&f))throw std::runtime_error("Cannot query framebuffer");
        if(v.xres!=320||v.yres!=170||(v.bits_per_pixel!=16&&v.bits_per_pixel!=32))throw std::runtime_error("Expected built-in 320 x 170 framebuffer, 16 or 32 bpp");
        pitch=f.line_length;bpp=v.bits_per_pixel;xoffset=v.xoffset;yoffset=v.yoffset;fbsize=f.smem_len;
        red_offset=v.red.offset;red_length=v.red.length;green_offset=v.green.offset;green_length=v.green.length;blue_offset=v.blue.offset;blue_length=v.blue.length;
        if((size_t(yoffset)+170)*pitch>fbsize)throw std::runtime_error("Framebuffer geometry exceeds mapping");
        fbmem=mmap(nullptr,fbsize,PROT_READ|PROT_WRITE,MAP_SHARED,fb,0);if(fbmem==MAP_FAILED)throw std::runtime_error("Framebuffer map failed");
        init_keyboard();
        auto wanted=getenv("FROTZ_ZERO_INPUT");
        for(int i=0;i<64;i++){std::string p="/dev/input/event"+std::to_string(i);if(wanted&&p!=wanted)continue;int fd=open(p.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)continue;
            unsigned long bits[(KEY_MAX+8*sizeof(long))/(8*sizeof(long))]{};ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(bits)),bits);
            auto has=[&](int k){return bits[k/(8*sizeof(long))]&(1UL<<(k%(8*sizeof(long))));};if(has(KEY_A)&&has(KEY_ENTER))keyboards.push_back(fd);else close(fd);}
        if(keyboards.empty())throw std::runtime_error("No readable keyboard. Check input group/ACL in the device report.");
    }else{
        window=SDL_CreateWindow("Frotz Zero",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,320*std::clamp(scale,1,5),170*std::clamp(scale,1,5),SDL_WINDOW_RESIZABLE);
        renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer)throw std::runtime_error(SDL_GetError());
        SDL_RenderSetLogicalSize(renderer,320,170);texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,170);SDL_StartTextInput();
    }clear();present();
}
Display::~Display(){for(int fd:keyboards)close(fd);if(key_state)xkb_state_unref(static_cast<xkb_state*>(key_state));if(key_map)xkb_keymap_unref(static_cast<xkb_keymap*>(key_map));if(key_context)xkb_context_unref(static_cast<xkb_context*>(key_context));if(fbmem&&fbmem!=MAP_FAILED)munmap(fbmem,fbsize);if(fb>=0)close(fb);if(texture)SDL_DestroyTexture(texture);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);if(canvas)SDL_FreeSurface(canvas);for(auto&f:reading_fonts)TTF_CloseFont(f.second);TTF_CloseFont(font);TTF_CloseFont(heading);TTF_CloseFont(small);TTF_Quit();SDL_Quit();}
TTF_Font* Display::ui_font(int size)const{
    if(size==17)return heading;if(size<=10)return small;if(size<=12)return font;
    size=std::clamp(size,14,24);auto& cached=reading_fonts[size];
    if(!cached){cached=TTF_OpenFont(font_path().c_str(),size);if(!cached)throw std::runtime_error("Cannot load reading font");}return cached;
}
void Display::theme(bool d){dark=d;bg=d?0xff0d1b2a:0xfff5f1e7;fg=d?0xfff5f2e7:0xff18323f;muted=d?0xffb0c2cb:0xff51646d;accent=d?0xff74dfc4:0xff006b5d;panel=d?0xff182d40:0xfffffdf7;gold=d?0xffe9bd75:0xff8b5a16;}
void Display::clear(){SDL_FillRect(canvas,nullptr,bg);}
void Display::rect(int x,int y,int w,int h,uint32_t c){SDL_Rect r{x,y,w,h};SDL_FillRect(canvas,&r,c);}
void Display::rounded_rect(int x,int y,int w,int h,int radius,uint32_t c){
    radius=std::clamp(radius,0,std::min(w,h)/2);
    rect(x+radius,y,w-2*radius,h,c);rect(x,y+radius,w,h-2*radius,c);
    for(int row=0;row<radius;row++){int inset=radius-int(std::sqrt(radius*radius-(radius-row-1)*(radius-row-1)));rect(x+inset,y+row,w-2*inset,1,c);rect(x+inset,y+h-row-1,w-2*inset,1,c);}
}
void Display::text(int x,int y,const std::string&s,uint32_t c,int size,int max_width,int offset){if(s.empty())return;if(!c)c=fg;auto f=ui_font(size);SDL_Color col{uint8_t(c>>16),uint8_t(c>>8),uint8_t(c),255};auto surf=TTF_RenderUTF8_Blended(f,s.c_str(),col);if(!surf)return;offset=std::clamp(offset,0,surf->w);SDL_Rect src{offset,0,std::max(0,std::min(max_width,surf->w-offset)),surf->h},dst{x,y,src.w,src.h};SDL_BlitSurface(surf,&src,canvas,&dst);SDL_FreeSurface(surf);}
int Display::text_width(const std::string&s,int size)const{int w=0;TTF_SizeUTF8(ui_font(size),s.c_str(),&w,nullptr);return w;}
std::vector<std::string> Display::wrap(const std::string&s,int width,int size,std::vector<size_t>*offsets)const{
    std::vector<std::string> out;std::istringstream paragraphs(s);std::string paragraph;size_t base=0;
    if(offsets)offsets->clear();
    auto emit=[&](const std::string&line,size_t start){out.push_back(line);if(offsets)offsets->push_back(start);};
    while(std::getline(paragraphs,paragraph)){
        std::istringstream in(paragraph);std::string word,line;size_t at=0,start=base;
        if(paragraph.empty())emit("",base);
        while(in>>word){size_t word_at=paragraph.find(word,at);at=word_at+word.size();word_at+=base;
            if(!line.empty()&&text_width(line+" "+word,size)>width){emit(line,start);line.clear();}
            while(text_width(word,size)>width){size_t end=0,next=0;while(next<word.size()){next++;while(next<word.size()&&(static_cast<unsigned char>(word[next])&0xc0)==0x80)next++;if(text_width(word.substr(0,next),size)>width&&end)break;end=next;}emit(word.substr(0,end),word_at);word.erase(0,end);word_at+=end;}
            if(!word.empty()){if(!line.empty())line+=' ';else start=word_at;line+=word;}}
        if(!line.empty())emit(line,start);base+=paragraph.size()+1;
    }return out;
}
void Display::paragraph(int x,int y,const std::string&s,int width,int lines){auto rows=wrap(s,width);for(int i=0;i<std::min(lines,int(rows.size()));i++)text(x,y+i*16,rows[i],0,12,width);}
void Display::present(){
    if(!capture_once.empty()){screenshot(capture_once);capture_once.clear();capture_exit=true;}
    if(!device){SDL_UpdateTexture(texture,nullptr,canvas->pixels,canvas->pitch);SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,nullptr,nullptr);SDL_RenderPresent(renderer);return;}
    auto channel=[](uint32_t c,unsigned shift,unsigned len){return ((c*((1u<<len)-1)+127)/255)<<shift;};
    for(int y=0;y<170;y++){auto src=reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(canvas->pixels)+y*canvas->pitch);auto dst=static_cast<uint8_t*>(fbmem)+(y+yoffset)*pitch+xoffset*bpp/8;for(int x=0;x<320;x++){auto c=src[x];uint32_t v=channel((c>>16)&255,red_offset,red_length)|channel((c>>8)&255,green_offset,green_length)|channel(c&255,blue_offset,blue_length);memcpy(dst+x*bpp/8,&v,bpp/8);}}
}
void Display::screenshot(const fs::path&p){fs::create_directories(p.parent_path());auto converted=SDL_ConvertSurfaceFormat(canvas,SDL_PIXELFORMAT_RGBA32,0);if(!converted)throw std::runtime_error(SDL_GetError());png_image image{};image.version=PNG_IMAGE_VERSION;image.width=320;image.height=170;image.format=PNG_FORMAT_RGBA;bool ok=png_image_write_to_file(&image,p.c_str(),0,converted->pixels,converted->pitch,nullptr);SDL_FreeSurface(converted);if(!ok)throw std::runtime_error(image.message);}
void Display::load_drive(const fs::path&p){std::ifstream in(p);if(!in)throw std::runtime_error("Cannot open drive script");std::string line;uint64_t t=0;while(std::getline(in,line)){if(line.empty()||line[0]=='#')continue;if(line.rfind("wait ",0)==0)t+=std::stoul(line.substr(5));else {drive.push_back({t,line});t+=40;}}drive_start=SDL_GetTicks64();}
void Display::init_keyboard(){
    if(key_state)return;
    auto ctx=xkb_context_new(XKB_CONTEXT_NO_FLAGS);key_context=ctx;
    if(!ctx)throw std::runtime_error("Keyboard context unavailable");
    xkb_rule_names names{};names.layout="us";
    auto km=xkb_keymap_new_from_names(ctx,&names,XKB_KEYMAP_COMPILE_NO_FLAGS);key_map=km;
    if(!km)throw std::runtime_error("Keyboard map unavailable");
    key_state=xkb_state_new(km);if(!key_state)throw std::runtime_error("Keyboard state unavailable");
}
bool Display::keyboard_event(int code,int value,SDL_Event* e){
    if(code<0||code>KEY_MAX||value<0||value>2)throw std::runtime_error("Invalid keyboard event");
    // A key held while the library launches a story can arrive only as repeats
    // in the new process. Require a fresh press before accepting those repeats.
    if(value==2&&!pressed_keys.count(code))return false;
    if(value==1)pressed_keys.insert(code);else if(value==0)pressed_keys.erase(code);
    init_keyboard();auto state=static_cast<xkb_state*>(key_state);
    if(value!=2)xkb_state_update_key(state,code+8,value?XKB_KEY_DOWN:XKB_KEY_UP);
    if(!value)return false;
    uint16_t mods=0;
    if(xkb_state_mod_name_is_active(state,XKB_MOD_NAME_CTRL,XKB_STATE_MODS_EFFECTIVE)>0)mods|=KMOD_CTRL;
    if(xkb_state_mod_name_is_active(state,XKB_MOD_NAME_ALT,XKB_STATE_MODS_EFFECTIVE)>0)mods|=KMOD_ALT;
    if(xkb_state_mod_name_is_active(state,XKB_MOD_NAME_SHIFT,XKB_STATE_MODS_EFFECTIVE)>0)mods|=KMOD_SHIFT;
    auto key=evkey(code);char str[32]{};xkb_state_key_get_utf8(state,code+8,str,sizeof(str));
    if(!key&&(mods&(KMOD_CTRL|KMOD_ALT))){auto sym=xkb_state_key_get_one_sym(state,code+8);if(sym>=32&&sym<=126)key=std::tolower(sym);}
    memset(e,0,sizeof(*e));
    if(key){e->type=SDL_KEYDOWN;e->key.keysym.sym=key;e->key.keysym.mod=mods;e->key.repeat=value==2;return true;}
    if(str[0]&&static_cast<unsigned char>(str[0])>=32){e->type=SDL_TEXTINPUT;SDL_strlcpy(e->text.text,str,sizeof(e->text.text));return true;}
    return false;
}
bool Display::poll(SDL_Event*e,bool include_deferred){
    if(quit_requested){memset(e,0,sizeof(*e));e->type=SDL_QUIT;return true;}
    if(include_deferred&&!event_queue.empty()){*e=event_queue.front();event_queue.pop_front();return true;}
    if(capture_exit){capture_exit=false;memset(e,0,sizeof(*e));e->type=SDL_QUIT;return true;}
    if(!drive.empty()&&SDL_GetTicks64()-drive_start>=drive.front().first){
        // Keep scripted waits relative to the event actually being consumed.
        // Slow startup/fsync must not collapse a later wait or capture a toast
        // before its intended delay, especially during parallel build tests.
        drive_start=SDL_GetTicks64()-drive.front().first;
        auto cmd=drive.front().second;drive.pop_front();memset(e,0,sizeof(*e));
        if(cmd.rfind("capture ",0)==0){screenshot(cmd.substr(8));return false;}
        if(cmd=="quit"){e->type=SDL_QUIT;return true;}
        if(cmd.rfind("text ",0)==0){e->type=SDL_TEXTINPUT;SDL_strlcpy(e->text.text,cmd.substr(5).c_str(),sizeof(e->text.text));return true;}
        if(cmd.rfind("click ",0)==0){e->type=SDL_MOUSEBUTTONDOWN;std::istringstream in(cmd.substr(6));in>>e->button.x>>e->button.y;e->button.clicks=1;return true;}
        // Exercise the same Linux-keycode/xkb translation as the native keyboard.
        // These scripted events stay inside this process; no OS input is injected.
        if(cmd.rfind("evkey ",0)==0){int code,value;std::istringstream in(cmd.substr(6));if(!(in>>code>>value))throw std::runtime_error("Invalid evkey drive command");return keyboard_event(code,value,e);}
        if(cmd.rfind("key ",0)==0){auto k=cmd.substr(4);e->type=SDL_KEYDOWN;e->key.keysym.mod=0;while(k.rfind("ctrl+",0)==0||k.rfind("shift+",0)==0){if(k.rfind("ctrl+",0)==0){e->key.keysym.mod|=KMOD_CTRL;k=k.substr(5);}else{e->key.keysym.mod|=KMOD_SHIFT;k=k.substr(6);}}e->key.keysym.sym=SDL_GetKeyFromName(k.c_str());return true;}}
    if(SDL_PollEvent(e))return true;
    if(!device)return false;
    for(int fd:keyboards){input_event v{};while(read(fd,&v,sizeof(v))==sizeof(v))if(v.type==EV_KEY&&keyboard_event(v.code,v.value,e))return true;}
    return false;
}
Key Display::wait(int timeout){auto until=SDL_GetTicks64()+timeout;SDL_Event e;do{if(poll(&e)){if(e.type==SDL_QUIT){quit_requested=true;return {0,0,"",true};}if(e.type==SDL_KEYDOWN)return {e.key.keysym.sym,e.key.keysym.mod,"",false};if(e.type==SDL_TEXTINPUT)return {0,0,e.text.text,false};if(e.type==SDL_WINDOWEVENT)present();}SDL_Delay(8);}while(SDL_GetTicks64()<until);return {};}
bool menu_back(const Key&k){return k.quit||k.code==SDLK_ESCAPE||k.code==SDLK_LEFT||k.text=="z";}
bool menu_accept(const Key&k){return k.code==SDLK_RETURN||k.code==SDLK_KP_ENTER||k.code==SDLK_RIGHT||k.text=="c";}
int menu_step(const Key&k){return k.code==SDLK_UP||k.text=="f"?-1:k.code==SDLK_DOWN||k.text=="x"||k.code==SDLK_TAB?1:0;}
int menu_digit(const Key&k){return k.text.size()==1&&k.text[0]>='1'&&k.text[0]<='9'?k.text[0]-'0':0;}
int menu(Display&d,const std::string&title,const std::vector<std::string>&items,const std::string&hint,int selected,int text_size){if(items.empty())return -1;int sel=std::clamp(selected,0,int(items.size())-1);uint64_t since=SDL_GetTicks64();std::vector<int> widths;for(auto&s:items)widths.push_back(d.text_width(s,text_size));int last_sel=-1,last_shift=-1;
    while(true){int top=std::min(std::max(0,sel-2),std::max(0,int(items.size())-4));int shift=widths[sel]>266&&SDL_GetTicks64()-since>1000?int((SDL_GetTicks64()-since-1000)/35)%(widths[sel]-266+30):0;shift=std::min(shift,std::max(0,widths[sel]-266));
      if(last_sel!=sel||last_shift!=shift){d.clear();page_header(d,title,std::to_string(sel+1)+"/"+std::to_string(items.size()));
        for(int i=top;i<std::min(top+4,int(items.size()));i++){int y=36+(i-top)*27;if(i==sel){d.rounded_rect(7,y,306,25,5,d.gold);d.rounded_rect(8,y+1,304,23,4,d.panel);d.rect(12,y+7,2,11,d.accent);}d.text(20,y+4-(text_size-12)/2,items[i],i==sel?d.fg:d.muted,text_size,266,i==sel?shift:0);d.text(298,y+6,std::to_string(i-top+1),d.gold,10,10);}
        d.text(10,153,hint,d.muted,10,302);d.present();last_sel=sel;last_shift=shift;}auto k=d.wait();if(menu_back(k))return -1;if(menu_accept(k))return sel;
        int digit=menu_digit(k);if(digit&&digit<=4&&top+digit<=int(items.size()))return top+digit-1;
        int step=menu_step(k);if(step){sel=(sel+int(items.size())+step)%items.size();since=SDL_GetTicks64();}if(k.code==SDLK_HOME)sel=0;if(k.code==SDLK_END)sel=items.size()-1;if(k.code==SDLK_PAGEUP)sel=std::max(0,sel-4);if(k.code==SDLK_PAGEDOWN)sel=std::min(int(items.size())-1,sel+4);
    }
}
void message(Display&d,const std::string&t,const std::string&s){
    auto rows=d.wrap(s,294);int top=0,last=-1,limit=std::max(0,int(rows.size())-6);
    for(;;){
        if(last!=top){
            d.clear();page_header(d,t);
            for(int i=0;i<6&&top+i<int(rows.size());i++)d.text(10,36+i*17,rows[top+i],d.fg,12,294);
            if(limit){d.rect(313,38,2,98,d.panel);int thumb=std::max(5,98*6/int(rows.size()));d.rect(313,38+(98-thumb)*top/limit,2,thumb,d.accent);}
            d.text(10,153,limit?"F/X scroll   Fn+L/M page   Enter/Esc back":"Enter or Esc to return",d.muted,10,302);
            d.present();last=top;
        }
        auto k=d.wait();if(menu_back(k)||menu_accept(k))return;
        if(k.code==SDLK_DOWN||k.text=="x")top=std::min(limit,top+1);
        if(k.code==SDLK_UP||k.text=="f")top=std::max(0,top-1);
        if(k.code==SDLK_PAGEDOWN)top=std::min(limit,top+6);
        if(k.code==SDLK_PAGEUP)top=std::max(0,top-6);
        if(k.code==SDLK_HOME)top=0;
        if(k.code==SDLK_END)top=limit;
    }
}
std::string edit(Display&d,const std::string&t,const std::string&initial,size_t limit){std::string value=initial;size_t pos=value.size();while(true){d.clear();d.text(10,6,t,0,17);d.paragraph(10,38,"Type a value. Left/right move the cursor; Backspace and Delete edit.",300,3);d.rect(8,94,304,27,d.panel);auto shown=value;shown.insert(pos,"|");if(shown.size()>40){size_t start=pos>36?pos-36:0;shown=shown.substr(start,40);}d.text(13,99,shown);d.text(10,151,"Enter accept   Esc cancel",d.muted);d.present();auto k=d.wait();if(k.quit||k.code==SDLK_ESCAPE)return {};if(k.code==SDLK_RETURN)return value;if(k.code==SDLK_LEFT&&pos)pos--;if(k.code==SDLK_RIGHT&&pos<value.size())pos++;if(k.code==SDLK_HOME)pos=0;if(k.code==SDLK_END)pos=value.size();if(k.code==SDLK_BACKSPACE&&pos)value.erase(--pos,1);if(k.code==SDLK_DELETE&&pos<value.size())value.erase(pos,1);if(!k.text.empty()&&value.size()+k.text.size()<=limit){value.insert(pos,k.text);pos+=k.text.size();}}}
void settings_menu(Display& d,Settings& s,const Paths& p,bool playing){
    int sel=0;
    for(;;){
        std::vector<std::string> rows={"Theme: "+s.theme,"Reading size: "+std::to_string(s.text_size)+" px",
            std::string("Mute: ")+(s.mute?"on":"off"),"Volume: "+std::to_string(s.volume)+"%","Story folder","Back"};
        sel=menu(d,"Settings",rows,playing?"Enter change   Size applies on return":"Enter change   Esc back",sel);
        if(sel<0||sel==5)return;
        if(sel==0){s.theme=s.theme=="dark"?"light":"dark";d.theme(s.theme=="dark");}
        if(sel==1)s.text_size=s.text_size>=24?12:s.text_size+2;
        if(sel==2)s.mute=!s.mute;
        if(sel==3)s.volume=(s.volume+10)%110;
        if(sel==4){
            auto f=edit(d,"Story folder",s.stories.string());
            if(!f.empty()){std::error_code ec;if(fs::is_directory(f,ec))s.stories=fs::absolute(f);else message(d,"Folder unavailable","Choose an existing readable folder. Your current folder is unchanged.");}
        }
        try{write_settings(p,s);}catch(const std::exception& e){message(d,"Settings not saved",e.what());}
    }
}
}
