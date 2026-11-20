// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform.hpp"
#include "artwork.hpp"
#include <algorithm>

namespace fz {
namespace {
struct ControlRow {std::string key,action;};
struct ControlTopic {std::string title;std::vector<ControlRow> rows;};

const std::vector<ControlTopic> topics={
    {"Reading",{
        {"Ctrl+U","Larger text"},
        {"Ctrl+D","Smaller text"},
        {"Tab","Text / original view"},
        {"Ctrl+V","Display options"},
        {"Ctrl+0","Fit / Automatic"},
        {"Fn+L/M","Page back / forward"},
        {"Space/Enter","Page untimed text"},
        {"Timed text","Timers keep running"},
        {"Ctrl+F/X","Pan up / down"},
        {"Ctrl+Z/C","Pan left / right"},
        {"Original view","Art, maps and status"},
        {"Automatic","Chosen text size"}
    }},
    {"Game controls",{
        {"Ctrl+Esc","Game menu"},
        {"Fn+H","Controls help"},
        {"Ctrl+P","Previous command"},
        {"Ctrl+N","Next / draft"},
        {"Fn+Z/C","Cursor left / right"},
        {"Fn+K/N","Line start / end"},
        {"Backspace","Delete left"},
        {"Fn+A","Mute on / off"},
        {"Fn+J","Private screenshot"},
        {"Fn+L/M","Read older text"},
        {"Fn+N / Esc","Leave scrollback"},
        {"Plain keys","Type into the story"}
    }},
    {"Menus",{
        {"Arrows","Move selection"},
        {"Enter/Right","Open selection"},
        {"Esc/Left","Go back"},
        {"Numbers","Visible shortcuts"},
        {"F/X/Z/C","Arrows without Fn"},
        {"R","Refresh Library"},
        {"O","Library downloads"},
        {"Tab","Story details"},
        {"Home: 1","Continue last story"},
        {"Story: 4","Confirm delete; saves stay"}
    }},
    {"Saves",{
        {"Story: 3","Open named saves"},
        {"M / Tab","Save actions"},
        {"Enter / 1-3","Load a listed save"},
        {"Type SAVE","Portable story save"},
        {"Type RESTORE","Load portable save"},
        {"Game menu","Save checkpoint"},
        {"Rename","Keep type / contents"},
        {"Delete","Confirm removal"},
        {"Continue","Automatic, protected"},
        {"V6 / timed","Use story SAVE"},
        {"Re-add title","Same release finds saved games"}
    }},
    {"Keyboard",{
        {"Fn+F / Fn+X","Up / down"},
        {"Fn+Z / Fn+C","Left / right"},
        {"Fn+K / Fn+N","Home / End"},
        {"Fn+L / Fn+M","Page up / down"},
        {"Fn+Bksp","Delete right"},
        {"Ctrl+letter","No Fn / Sym / Shift"},
        {"Ctrl+Space","Pointer on / off"},
        {"Arrows","Move the pointer"},
        {"Enter / Esc","Click / cancel pointer"},
        {"Tab, Enter","Wake the launcher"}
    }}
};

void control_topic(Display&d,const ControlTopic&topic){
    struct Row {std::vector<std::string> key,action;int height;};
    std::vector<std::vector<Row>> pages(1);int used=0;
    // Lay out complete rows before paging, so neither a shortcut nor its
    // meaning is split or clipped by the bottom of the 320x170 display.
    for(const auto&item:topic.rows){
        Row row{d.wrap(item.key,104,14),d.wrap(item.action,182,16),0};
        row.height=std::max({32,int(row.key.size())*17+10,int(row.action.size())*19+8});
        if(used+row.height>112&&!pages.back().empty()){pages.emplace_back();used=0;}
        pages.back().push_back(std::move(row));used+=pages.back().back().height;
    }
    int page=0,last=-1;
    for(;;){
        if(page!=last){
            d.clear();page_header(d,topic.title,std::to_string(page+1)+"/"+std::to_string(pages.size()));
            int y=35;
            for(const auto&row:pages[page]){
                int height=int(row.key.size())*17+6;
                int key_y=y+(row.height-height)/2;
                d.rounded_rect(9,key_y,116,height,5,d.panel);
                for(size_t i=0;i<row.key.size();i++)d.text(15,key_y+3+int(i)*17,row.key[i],d.accent,14,104);
                int action_y=y+(row.height-int(row.action.size())*19)/2;
                for(size_t i=0;i<row.action.size();i++)d.text(132,action_y+int(i)*19,row.action[i],d.fg,16,182);
                y+=row.height;
            }
            d.rect(9,149,302,1,d.panel);
            d.text(10,153,"Up/Down pages   Enter/Esc topics",d.muted,12,302);
            d.present();last=page;
        }
        auto k=d.wait();
        if(menu_back(k)||menu_accept(k))return;
        int step=menu_step(k);
        if(k.code==SDLK_PAGEUP)step=-1;
        if(k.code==SDLK_PAGEDOWN)step=1;
        if(step)page=std::clamp(page+step,0,int(pages.size())-1);
        if(k.code==SDLK_HOME)page=0;
        if(k.code==SDLK_END)page=int(pages.size())-1;
        int digit=menu_digit(k);if(digit&&digit<=int(pages.size()))page=digit-1;
    }
}
}

void help(Display&d){
    std::vector<std::string> choices;
    for(const auto&topic:topics)choices.push_back(topic.title);
    choices.push_back("Back");
    int selected=0;
    for(;;){
        selected=menu(d,"Controls",choices,"Arrows choose   Number/Enter open   Esc back",selected,16);
        if(selected<0||selected==int(topics.size()))return;
        control_topic(d,topics[selected]);
    }
}
}
