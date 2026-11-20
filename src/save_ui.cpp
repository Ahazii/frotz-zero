// SPDX-License-Identifier: GPL-2.0-or-later
#include "app.hpp"
#include "artwork.hpp"
#include <algorithm>
#include <ctime>
namespace fz {
namespace {
std::string save_date(const fs::path&file){
    std::error_code ec;auto time=fs::last_write_time(file,ec);if(ec)return "Date unavailable";
    auto sys=std::chrono::time_point_cast<std::chrono::system_clock::duration>(time-fs::file_time_type::clock::now()+std::chrono::system_clock::now());
    auto raw=std::chrono::system_clock::to_time_t(sys);char date[40];strftime(date,sizeof(date),"%d %b %Y %H:%M",localtime(&raw));return date;
}
bool loadable(Display&d,const NamedSave&save){
    if(save.valid){if(!save.issue.empty())message(d,"Checkpoint recovery",save.issue);return true;}
    message(d,"Cannot load this save",save.issue+"\n\nThe original is preserved. M opens Rename and Delete.");return false;
}
}
fs::path choose_save(Display&d,const fs::path&folder,const Story&story){
    int sel=0;fs::path focus;
    for(;;){
        auto saves=named_saves(folder,story);
        if(saves.empty()){message(d,"No named saves","Use the story's SAVE command for a portable Quetzal save, or Save checkpoint at a supported line prompt. Automatic Continue is separate and protected.");return {};}
        sel=std::clamp(sel,0,int(saves.size())-1);
        for(size_t i=0;i<saves.size();i++)if(saves[i].path==focus)sel=int(i);
        int last=-1,last_shift=-1;auto since=SDL_GetTicks64();bool refresh=false;
        while(!refresh){
            auto&save=saves[sel];auto name=save.path.stem().string();int width=d.text_width(name),shift=0;
            if(width>274&&SDL_GetTicks64()-since>1000)shift=std::min(int((SDL_GetTicks64()-since-1000)/38)%(width-274+30),width-274);
            int top=std::clamp(sel-1,0,std::max(0,int(saves.size())-3));
            if(last!=sel||last_shift!=shift){
                d.clear();page_header(d,"Saved games",std::to_string(sel+1)+"/"+std::to_string(saves.size()));
                d.text(10,27,story.title,d.muted,10,300);
                for(int row=0;row<3&&top+row<int(saves.size());row++){
                    auto&item=saves[top+row];int y=44+row*34;bool selected=top+row==sel;
                    d.rounded_rect(7,y,306,32,4,selected?d.gold:d.panel);if(selected)d.rounded_rect(8,y+1,304,30,3,d.panel);
                    d.text(13,y+4,std::to_string(row+1),d.gold,10,10);
                    d.text(28,y+2,item.path.stem().string(),item.valid?d.fg:d.muted,12,274,selected?shift:0);
                    d.text(28,y+17,save_date(item.path),d.muted,10,173);
                    std::string type=item.checkpoint?"Checkpoint":"Quetzal";if(!item.valid)type+=" !";
                    d.text(305-d.text_width(type,10),y+17,type,item.valid?d.accent:d.gold,10,115);
                }
                d.text(10,154,"Enter / 1-3 load   M manage   Esc back",d.muted,10,300);d.present();last=sel;last_shift=shift;
            }
            auto k=d.wait();if(menu_back(k))return {};
            int digit=menu_digit(k);bool load=menu_accept(k);
            if(digit&&digit<=3&&top+digit<=int(saves.size())){sel=top+digit-1;load=true;}
            if(load){if(loadable(d,saves[sel]))return saves[sel].path;last=-1;continue;}
            if(k.text=="m"||k.text=="M"||k.code==SDLK_TAB){
                focus=save.path;
                int action=menu(d,save.path.stem().string(),{"Load save","Rename save","Delete save","Back"},std::string(save.checkpoint?"Checkpoint":"Quetzal")+"   Automatic Continue protected");
                if(action==0){if(loadable(d,save))return save.path;}
                if(action==1){
                    auto name=edit(d,"Rename save",save.path.stem().string(),48);
                    if(!name.empty())try{focus=rename_named_save(folder,save,name);message(d,"Save renamed",focus.filename().string()+"\n\nThe save contents and type are unchanged.");}catch(const std::exception&e){message(d,"Rename stopped",e.what());}
                }
                if(action==2&&menu(d,"Delete named save?",{"Cancel","Delete "+save.path.stem().string()},"Permanent; other saves and Continue kept")==1){
                    try{delete_named_save(folder,save);focus.clear();message(d,"Save deleted","The selected named save was removed. Other saves and automatic Continue were kept.");}catch(const std::exception&e){message(d,"Deletion stopped",e.what());}
                }
                refresh=true;continue;
            }
            int previous=sel,step=menu_step(k);if(step)sel=(sel+int(saves.size())+step)%saves.size();
            if(k.code==SDLK_HOME)sel=0;if(k.code==SDLK_END)sel=saves.size()-1;
            if(k.code==SDLK_PAGEUP)sel=std::max(0,sel-3);if(k.code==SDLK_PAGEDOWN)sel=std::min(int(saves.size())-1,sel+3);
            if(previous!=sel)since=SDL_GetTicks64();
        }
    }
}
}
