// SPDX-License-Identifier: GPL-3.0-or-later
#include "online_ui.hpp"
#include "online.hpp"
#include "qrcodegen.h"
#include <algorithm>
#include <array>
#include <thread>
namespace fz {
namespace {
// The worker only touches its own buffers and atomics. All SDL drawing and
// input stay on the main thread, and leaving the screen always joins it.
bool transfer(Display&d,const OnlineRelease*release,const fs::path&fixture,std::vector<uint8_t>&bytes,OnlineSource source=OnlineSource::Infocom){
    if(release)source=release->source;
    Transfer status;std::atomic<bool>done{false};std::string error;
    std::thread worker([&]{try{bytes=fetch_online(release,status,fixture,source);}catch(const std::exception&e){error=e.what();}done=true;});
    try{
        do{
            auto received=status.bytes.load(),total=status.total.load();
            d.clear();d.text(10,8,status.cancel?"Cancelling...":"Connecting to library",d.fg,17);
            d.text(10,37,source==OnlineSource::Infocom?"eblong.com / Infocom":"ifarchive.org / Z-code",d.accent,12);
            d.paragraph(10,61,release?release->filename:"Fetching the list of titles and releases",300,3);
            d.rounded_rect(10,112,300,7,3,d.panel);
            if(total)d.rect(10,112,int(300*std::min(received,total)/total),7,d.accent);
            else d.rect(10+int(SDL_GetTicks64()/12%265),112,35,7,d.accent);
            d.text(10,129,std::to_string(received/1024)+" KB"+(total?" / "+std::to_string(total/1024)+" KB":""),d.muted,10);
            d.text(10,153,status.cancel?"Waiting for the connection to close":"Esc cancel   Your library stays available",d.muted,10);d.present();
            auto k=d.wait(50);if(menu_back(k))status.cancel=true;
        }while(!done);
    }catch(...){status.cancel=true;worker.join();throw;}
    worker.join();if(status.cancel)return false;
    if(!error.empty()){message(d,"Could not connect",error+"\n\nCached titles and installed stories remain available. Check Wi-Fi, then try Refresh catalogue.");return false;}
    return true;
}
void qr_link(Display&d,const std::string&title,const std::string&url){
    std::array<uint8_t,qrcodegen_BUFFER_LEN_FOR_VERSION(10)>temp{},qr{};
    if(!qrcodegen_encodeText(url.c_str(),temp.data(),qr.data(),qrcodegen_Ecc_MEDIUM,1,10,qrcodegen_Mask_AUTO,true)){message(d,title,url);return;}
    int size=qrcodegen_getSize(qr.data()),scale=std::min(3,128/(size+8)),side=(size+8)*scale;
    d.clear();d.text(10,5,title,d.fg,17);d.rect(8,30,side,side,0xffffffff);
    for(int y=0;y<size;y++)for(int x=0;x<size;x++)if(qrcodegen_getModule(qr.data(),x,y))d.rect(8+(x+4)*scale,30+(y+4)*scale,scale,scale,0xff000000);
    d.paragraph(146,34,"Scan with your phone to open this page.",166,3);
    auto lines=d.wrap(url,166,10);for(size_t i=0;i<std::min(size_t(5),lines.size());i++)d.text(146,86+i*12,lines[i],d.accent,10,166);
    d.text(10,157,"Enter / Esc return",d.muted,10);d.present();
    for(;;){auto k=d.wait();if(menu_back(k)||menu_accept(k))return;}
}
}
void online_links(Display&d){
    const std::array<std::pair<const char*,const char*>,5>items={{{"Infocom catalogue",online_url},{"Game manuals","https://infodoc.plover.net/manuals/index.html"},{"Accessible manuals","https://infodoc.plover.net/screenread/index.html"},{"Media archive","https://ifarchive.org/indexes/if-archive/infocom/media/"},{"IF Archive Z-code",archive_url}}};
    int sel=0;for(;;){sel=menu(d,"Manuals & website",{"Infocom catalogue","Game manuals","Accessible manuals","Media archive","IF Archive Z-code","Back"},"Right/Enter link + QR   Left back",sel);if(sel<0||sel==5)return;qr_link(d,items[sel].first,items[sel].second);}
}
namespace {
int release_screen(Display&d,const OnlineRelease&r,bool installed){
    int sel=0,last=-1,last_shift=-1;auto since=SDL_GetTicks64();auto width=d.text_width(r.title,17);
    auto notes=d.wrap(r.notes.empty()?"No release notes supplied. Read details before choosing this release.":r.notes,300);
    if(notes.size()>2){while(!notes[1].empty()&&d.text_width(notes[1]+"...")>300)notes[1].pop_back();notes[1]+="...";}
    for(;;){
        int shift=width>300&&SDL_GetTicks64()-since>1000?std::min(int((SDL_GetTicks64()-since-1000)/38)%(width-300+35),width-300):0;
        if(last!=sel||last_shift!=shift){
            d.clear();d.text(10,5,r.title,d.fg,17,300,shift);d.text(10,28,release_label(r),d.accent,10);
            for(size_t i=0;i<std::min(size_t(2),notes.size());i++)d.text(10,45+15*i,notes[i],d.fg,12,300);
            const std::array<std::string,3>actions={installed?"Use installed story":"Download story","Read all release notes","Manuals & website"};
            for(int i=0;i<3;i++){int y=86+i*22;d.rounded_rect(8,y,304,21,4,i==sel?d.accent:d.panel);if(i==sel)d.rounded_rect(9,y+1,302,19,3,d.panel);d.text(17,y+3,actions[i],i==sel?d.fg:d.muted,12);d.text(297,y+4,std::to_string(i+1),d.gold,10);}
            d.text(10,157,"Right/Enter select   1-3   Left back",d.muted,10);d.present();last=sel;last_shift=shift;
        }
        auto k=d.wait();if(menu_back(k))return -1;if(menu_accept(k))return sel;auto digit=menu_digit(k);if(digit&&digit<=3)return digit-1;
        int step=menu_step(k);if(step)sel=(sel+3+step)%3;
    }
}
bool installed(const OnlineRelease&r,const fs::path&folder){try{auto p=folder/r.filename;return !fs::is_symlink(p)&&md5(read_bytes(p,archive_download_limit))==r.md5;}catch(...){return false;}}
std::string browse(Display&d,const OnlineCatalog&catalog,const Settings&s,const Paths&p,const fs::path&fixture,const std::string&query){
    std::vector<std::string>names;std::vector<const OnlineTitle*>titles;
    auto lower=[](std::string value){for(char&c:value)c=std::tolower(static_cast<unsigned char>(c));return value;};
    for(const auto&t:catalog)if(query.empty()||lower(t.title).find(lower(query))!=std::string::npos){names.push_back(t.title+" ("+std::to_string(t.releases.size())+")");titles.push_back(&t);}
    if(titles.empty()){message(d,"No matching titles","Try a shorter part of the title.");return {};}
    int ti=0;
    for(;;){
        ti=menu(d,"Online titles",names,"Enter releases   Esc library",ti);if(ti<0)return {};
        const auto&title=*titles[ti];std::vector<std::string>releases;for(const auto&r:title.releases)releases.push_back(release_label(r));
        int ri=0;
        for(;;){
            ri=menu(d,title.title,releases,"* collection release   Enter details",ri);if(ri<0)break;
            const auto&r=title.releases[ri];
            for(;;){
                auto action=release_screen(d,r,installed(r,s.stories));if(action<0)break;
                if(action==1){message(d,"Release notes",release_notes(r));continue;}
                if(action==2){online_links(d);continue;}
                try{
                    std::vector<uint8_t>bytes;
                    if(installed(r,s.stories))bytes=read_bytes(s.stories/r.filename,archive_download_limit);
                    else if(!transfer(d,&r,fixture,bytes))continue;
                    auto stories=r.source==OnlineSource::IFArchive?install_archive_release(r,bytes,s.stories):std::vector<Story>{install_online_story(r,bytes,s.stories)};
                    for(const auto&story:stories)remember_online_story(p,r,story);
                    message(d,"Story ready",r.title+"\n\nSaved in your library. Choose New Game on the story page to play."
                        +(r.version==6?std::string("\n\nA matching Blorb may be needed for graphics. See Manuals & website > Media archive."):std::string()));
                    if(stories.size()==1)return stories[0].id;
                    std::vector<std::string> names;for(const auto&story:stories)names.push_back(story.title);
                    auto selected=menu(d,"Stories in this bundle",names,"Right/Enter open   Left back");return selected<0?std::string():stories[selected].id;
                }catch(const std::exception&e){message(d,"Download not installed",e.what());}
            }
        }
    }
}
}
static std::string source_library(Display&d,const Settings&s,const Paths&p,const fs::path&fixture,OnlineSource source){
    OnlineCatalog catalog;try{catalog=cached_online_catalog(p,source);}catch(...){}
    auto refresh=[&](){std::vector<uint8_t>bytes;if(!transfer(d,nullptr,fixture,bytes,source))return false;try{catalog=save_online_catalog(p,bytes,source);return true;}catch(const std::exception&e){message(d,"Catalogue not updated",std::string(e.what())+"\n\nYour previous cached catalogue is unchanged.");return false;}};
    int sel=0;
    for(;;){
        sel=menu(d,source==OnlineSource::Infocom?"Infocom Library":"IF Archive",{"Browse titles"+(catalog.empty()?std::string():" ("+std::to_string(catalog.size())+" cached)"),"Search titles","Refresh catalogue","Manuals & website","About downloads","Back"},"Right/Enter select   1-4   Left back",sel);
        if(sel<0||sel==5)return {};
        if(sel==3){online_links(d);continue;}
        if(sel==4){message(d,"About downloads",std::string(source==OnlineSource::Infocom?"The public Infocom catalogue at eblong.com.":"The IF Archive Z-code collection, including language folders. Older releases are omitted. V3-V8 stories, Blorbs and ZIP bundles up to 32 MB are listed. Bundles with no supported story cannot be installed.")+"\n\nTitles and notes are cached offline. Downloads are checked against the catalogue checksum and never replace different files. Refresh updates the list; play needs no network.\n\nManual and website links show QR codes for a phone or PC. ZIP bundles keep supporting media and documents together. No web or PDF reader is included.");continue;}
        if(sel==2){if(refresh())message(d,"Catalogue updated",std::to_string(catalog.size())+" titles are ready to browse. This list is now available offline.");continue;}
        if(catalog.empty()&&!refresh())continue;
        std::string query;if(sel==1){query=edit(d,"Search title",{},80);if(query.empty())continue;}
        auto chosen=browse(d,catalog,s,p,fixture,query);if(!chosen.empty())return chosen;
    }
}
std::string online_library(Display&d,const Settings&s,const Paths&p,const fs::path&fixture){
    int selected=0;for(;;){selected=menu(d,"Download Stories",{"Infocom catalogue","IF Archive Z-code","Manuals & websites","Back"},"Right/Enter open   1-4   Left back",selected);
        if(selected<0||selected==3)return {};if(selected==2){online_links(d);continue;}
        auto chosen=source_library(d,s,p,fixture,selected==0?OnlineSource::Infocom:OnlineSource::IFArchive);if(!chosen.empty())return chosen;
    }
}
}
