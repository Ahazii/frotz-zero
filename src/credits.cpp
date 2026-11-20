// SPDX-License-Identifier: GPL-3.0-or-later
#include "credits.hpp"
#include "artwork.hpp"
#include "online_ui.hpp"
#include "credits.inc"
namespace fz {
namespace {
void credits(Display&d) {
    int selected=0;
    for(;;){
        selected=menu(d,"Credits & licences",{"Frotz Zero","Code authors","Libraries & sources","GPL licence","Manuals & website","Back"},"Enter read   Esc back",selected);
        if(selected<0||selected==5)return;
        if(selected==0)message(d,"Frotz Zero",std::string("Created by ")+project_author+"\n"+project_email+"\n\nA pocket home for interactive fiction, built for the M5Stack Cardputer Zero.\n\nPowered by Frotz and the open-source libraries credited here. Original book-and-lantern artwork: CC0-1.0. Application: GPL-3.0-or-later.\n\nStory and media rights remain with their respective owners.");
        if(selected==1)message(d,"Code authors",code_authors);
        if(selected==2)message(d,"Libraries & sources",third_party_notices);
        if(selected==3)message(d,"GPL licence",gpl_licence);
        if(selected==4)online_links(d);
    }
}
void about(Display&d) {
    for(;;){
    d.clear();page_header(d,"About Frotz Zero");
    d.rounded_rect(8,39,104,103,10,d.panel);brand(d,12,41,96);
    d.text(124,41,"Frotz Zero",d.fg,17,186);
    d.text(124,65,std::string("Version ")+FROTZ_ZERO_VERSION,d.gold,12,186);
    d.text(124,88,project_author,d.fg,12,186);
    d.text(124,107,project_email,d.accent,10,186);
    d.text(124,126,"A world in your pocket.",d.muted,10,186);
    d.text(10,155,"1 / Right / Enter credits   Left / Esc back",d.muted,10,300);d.present();
        for(;;){auto k=d.wait();if(menu_back(k))return;if(menu_accept(k)||menu_digit(k)==1){credits(d);break;}}
    }
}
}
void help_menu(Display&d) {
    int selected=0;
    for(;;){
        selected=menu(d,"Help",{"Controls","About Frotz Zero","Credits & licences","Back"},"Enter select   Esc home",selected);
        if(selected<0||selected==3)return;
        if(selected==0)help(d);
        if(selected==1)about(d);
        if(selected==2)credits(d);
    }
}
}
