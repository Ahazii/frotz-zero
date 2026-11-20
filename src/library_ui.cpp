// SPDX-License-Identifier: GPL-3.0-or-later
#include "library_ui.hpp"
#include "artwork.hpp"
#include <algorithm>
#include <array>
namespace fz {
namespace {
int marquee(int width,int available,uint64_t elapsed) {
    if(width<=available||elapsed<1000)return 0;
    return std::min(int((elapsed-1000)/38)%(width-available+40),width-available);
}
}
HomeAction home_screen(Display& d,const Story* story,size_t count,bool resumable,int selected) {
    const std::array<std::string,6> labels={"Continue","Library","Downloads","Settings","Help","Exit"};
    const std::array<Glyph,6> icons={Glyph::Continue,Glyph::Library,Glyph::LoadSave,Glyph::Settings,Glyph::Help,Glyph::Exit};
    const std::array<std::string,6> hints={
        resumable?"Resume your last played story":"No automatic resume; choose a story in Library",
        "Choose a story; R refreshes the library",
        "Browse Infocom and IF Archive downloads",
        "Theme, reading size, sound and story folder",
        "Controls, credits and the people behind Frotz",
        "Return to the device launcher"};
    int sel=std::clamp(selected,0,5),last=-1,last_shift=-1;
    auto subtitle=story?story->title:"No story played yet";
    const int width=d.text_width(subtitle);
    auto since=SDL_GetTicks64();
    for(;;){
        int shift=marquee(width,265,SDL_GetTicks64()-since);
        if(last!=sel||last_shift!=shift){
            d.clear();brand(d,6,-3,40);d.text(53,7,"Frotz Zero",d.fg,17,184);
            auto count_label=std::to_string(count)+" STORIES";
            d.text(310-d.text_width(count_label,10),11,count_label,d.gold,10);
            for(int i=0;i<6;i++){
                int x=(i==0||i==5)?8:8+((i-1)%2)*156,y=i==0?39:i==5?130:79+((i-1)/2)*25;
                int w=(i==0||i==5)?304:148,h=i==0?35:23;
                bool enabled=i!=0||resumable;
                d.rounded_rect(x,y,w,h,4,i==sel?d.gold:d.panel);
                if(i==sel)d.rounded_rect(x+1,y+1,w-2,h-2,3,d.panel);
                glyph(d,icons[i],x+8,y+(i==0?9:4),enabled?(i==sel?d.gold:d.accent):d.muted);
                d.text(x+32,y+3,labels[i],enabled?d.fg:d.muted,12,w-48);
                d.text(x+w-14,y+5,std::to_string(i+1),i==sel?d.gold:d.muted,10,10);
                if(i==0)d.text(x+32,y+18,subtitle,d.muted,12,265,shift);
            }
            d.text(9,155,hints[sel],d.muted,10,302);d.present();last=sel;last_shift=shift;
        }
        auto k=d.wait();
        if(k.quit||k.code==SDLK_ESCAPE)return HomeAction::Exit;
        if(k.code==SDLK_RETURN||k.code==SDLK_KP_ENTER)return static_cast<HomeAction>(sel);
        if(k.code==SDLK_LEFT||k.text=="z"){if(sel==2||sel==4)--sel;}
        if(k.code==SDLK_RIGHT||k.text=="c"){if(sel==1||sel==3)++sel;}
        if(k.code==SDLK_UP||k.text=="f"){const int up[]={5,0,0,1,2,3};sel=up[sel];}
        if(k.code==SDLK_DOWN||k.text=="x"){const int down[]={1,3,4,5,5,0};sel=down[sel];}
        if(k.code==SDLK_TAB)sel=(sel+(k.mods&KMOD_SHIFT?5:1))%6;
        if(k.code==SDLK_HOME)sel=0;
        if(k.code==SDLK_END)sel=5;
        int digit=menu_digit(k);if(digit&&digit<=6)return static_cast<HomeAction>(digit-1);
    }
}
StoryAction story_screen(Display&d,const Story&s,bool resumable){
    int sel=resumable?0:1,last=-1,last_shift=-1;auto since=SDL_GetTicks64();int width=d.text_width(s.title,17);
    auto rows=d.wrap(s.summary,198);if(rows.size()>5){auto&end=rows[4];while(!end.empty()&&d.text_width(end+"...")>198)end.pop_back();end+="...";}
    const std::array<std::string,4> labels={"Continue","New","Saves","Delete"};
    for(;;){int shift=marquee(width,302,SDL_GetTicks64()-since);
        if(last!=sel||last_shift!=shift){
            d.clear();d.text(8,4,s.title,d.fg,17,302,shift);d.rect(8,29,304,1,d.panel);d.rect(8,29,24,1,d.gold);
            story_art(d,s,8,35,96);
            for(int i=0;i<std::min(5,int(rows.size()));i++)d.text(114,34+i*15,rows[i],d.fg,12,198);
            d.text(114,113,"V"+std::to_string(s.version)+(s.pictures?"  Media":"")+(s.sound?"  Sound":""),d.accent,10,198);
            for(int i=0;i<4;i++){int x=8+i*78;d.rounded_rect(x,135,70,22,4,i==sel?d.gold:d.panel);if(i==sel)d.rounded_rect(x+1,136,68,20,3,d.panel);d.text(x+3,139,std::to_string(i+1),d.gold,10,8);d.text(x+13,138,labels[i],i==0&&!resumable?d.muted:d.fg,12,55);}
            const std::array<std::string,4> hints={resumable?"Resume play   Tab details   Esc library":"No automatic resume   Tab details   Esc library","Start a new game   Tab details   Esc library","Load a named save   Tab details   Esc library","Delete title files; keep saves   Esc library"};
            d.text(8,159,hints[sel],d.muted,10,304);d.present();last=sel;last_shift=shift;
        }
        auto k=d.wait();if(k.quit||k.code==SDLK_ESCAPE)return StoryAction::Back;
        if(k.code==SDLK_TAB){story_details(d,s);last=-1;continue;}
        if(k.code==SDLK_RETURN||k.code==SDLK_KP_ENTER)return static_cast<StoryAction>(sel);
        int digit=menu_digit(k);if(digit&&digit<=4)return static_cast<StoryAction>(digit-1);
        if(k.code==SDLK_LEFT||k.code==SDLK_UP||k.text=="z"||k.text=="f")sel=(sel+3)%4;
        if(k.code==SDLK_RIGHT||k.code==SDLK_DOWN||k.text=="c"||k.text=="x")sel=(sel+1)%4;
        if(k.code==SDLK_HOME)sel=0;if(k.code==SDLK_END)sel=3;
    }
}
void story_details(Display& d,const Story& story) {
    std::string body=story.title+"\n"+story.edition+"\n\n"+story.summary+
        "\n\nFile: "+story.path.filename().string()+"\nVersion "+std::to_string(story.version)+
        " | Release "+std::to_string(story.release)+" | Serial "+story.serial+"\nChecksum: "+story.checksum;
    if(!story.issue.empty())body+="\n\n"+story.issue;
    if(!story.resource.empty())body+="\n\nMedia: "+story.resource.filename().string()+(story.pictures?"\nPictures indexed":"")+(story.sound?"\nSound indexed":"");
    if(!story.catalog_source.empty())body+="\n\nIdentity source: "+story.catalog_source;
    message(d,"About this story",body);
}
int story_picker(Display& d,const std::vector<Story>& stories,int selected) {
    if(stories.empty()){int n=menu(d,"Library",{"Download Stories","Refresh library","Back"},"No local stories; choose a download");return n==0?-2:n==1?-3:-1;}
    int sel=std::clamp(selected,0,int(stories.size())-1),last=-1,last_title=-1,last_file=-1;
    auto since=SDL_GetTicks64();
    std::vector<int> title_widths,file_widths;
    for(const auto& story:stories){title_widths.push_back(d.text_width(story.title,17));file_widths.push_back(d.text_width(story.path.filename().string(),10));}
    for(;;){
        const auto& story=stories[sel];
        auto elapsed=SDL_GetTicks64()-since;
        int title_shift=marquee(title_widths[sel],303,elapsed),file_shift=marquee(file_widths[sel],270,elapsed);
        if(last!=sel||last_title!=title_shift||last_file!=file_shift){
            d.clear();glyph(d,Glyph::Library,9,3,d.gold);d.text(31,6,"LIBRARY",d.accent,10);
            d.text(101,6,"R refresh  O online",d.accent,10);
            auto count=std::to_string(sel+1)+" / "+std::to_string(stories.size());
            d.text(310-d.text_width(count,10),6,count,d.muted,10);
            d.text(8,23,story.title,d.fg,17,304,title_shift);d.rect(8,47,304,1,d.panel);d.rect(8,47,24,1,d.gold);
            int top=std::clamp(sel-1,0,std::max(0,int(stories.size())-4));
            for(int row=0;row<4&&top+row<int(stories.size());row++){
                int index=top+row,y=52+row*20;
                if(index==sel){d.rounded_rect(7,y-1,133,20,4,d.panel);d.rect(10,y+4,2,10,d.accent);}
                d.text(17,y+2,stories[index].title,index==sel?d.fg:d.muted,12,105);d.text(126,y+3,std::to_string(row+1),d.gold,10,10);
            }
            d.rect(146,53,1,77,d.panel);
            auto summary=d.wrap(story.summary,157);
            if(summary.size()>5){
                auto& end=summary[4];
                while(!end.empty()&&d.text_width(end+"...")>157)end.pop_back();
                end+="...";
            }
            for(int i=0;i<std::min(5,int(summary.size()));i++)d.text(155,51+i*15,summary[i],d.fg,12,157);
            std::string tag="V"+std::to_string(story.version);
            if(!story.valid)tag+="  Invalid story";
            else if(story.version==6&&!story.pictures)tag+="  Media needed";
            else if(!story.edition.empty())tag+="  "+story.edition;
            d.text(155,126,tag,d.accent,10,157);
            d.text(9,140,"File:",d.muted,10,30);d.text(35,140,story.path.filename().string(),d.muted,10,277,file_shift);
            d.text(9,156,"1-4 open   Right / Enter choose   Left back",d.muted,10,303);
            d.present();last=sel;last_title=title_shift;last_file=file_shift;
        }
        auto k=d.wait();
        if(menu_back(k))return -1;
        if(k.text=="o"||k.text=="O")return -2;
        if(k.text=="r"||k.text=="R")return -3;
        if(menu_accept(k))return sel;
        int digit=menu_digit(k),top=std::clamp(sel-1,0,std::max(0,int(stories.size())-4));if(digit&&digit<=4&&top+digit<=int(stories.size()))return top+digit-1;
        if(k.code==SDLK_TAB||k.text=="i"){story_details(d,story);last=-1;}
        int before=sel;
        if(k.code==SDLK_UP||k.text=="f")sel=(sel+stories.size()-1)%stories.size();
        if(k.code==SDLK_DOWN||k.text=="x")sel=(sel+1)%stories.size();
        if(k.code==SDLK_PAGEUP)sel=std::max(0,sel-4);
        if(k.code==SDLK_PAGEDOWN)sel=std::min(int(stories.size())-1,sel+4);
        if(k.code==SDLK_HOME)sel=0;
        if(k.code==SDLK_END)sel=stories.size()-1;
        if(before!=sel)since=SDL_GetTicks64();
    }
}
}
