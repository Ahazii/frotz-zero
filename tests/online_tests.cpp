// SPDX-License-Identifier: GPL-3.0-or-later
#include "online.hpp"
#include <nlohmann/json.hpp>
#include <cassert>
#include <iostream>
#include <thread>
using namespace fz;
template<class F>void rejects(F f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char**argv){
    // Separate test-only command exercises real libcurl against a local HTTPS
    // fixture and the public catalogue. It is not linked into the application.
    if(argc==7&&std::string(argv[1])=="fetch"){
        Transfer state;std::thread cancel;int ms=std::stoi(argv[5]);
        if(ms)cancel=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(ms));state.cancel=true;});
        int code=0;try{auto data=https_get(argv[2],state,std::stoul(argv[3]),std::string(argv[4])=="-"?fs::path():fs::path(argv[4]));atomic_write(argv[6],data);std::cout<<data.size()<<" verified HTTPS bytes\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';code=2;}
        if(cancel.joinable())cancel.join();return code;
    }
    if(argc==3&&std::string(argv[1])=="catalog"){auto list=parse_online_catalog(read_text(argv[2],catalog_limit));size_t count=0;for(auto&t:list)count+=t.releases.size();std::cout<<list.size()<<" titles / "<<count<<" supported releases\n";return 0;}
    auto dir=fs::temp_directory_path()/("fz-online-"+timestamp());fs::create_directories(dir);
    try{
        std::vector<uint8_t>b(1024);b[0]=5;b[3]=1;b[4]=3;b[6]=3;b[8]=2;b[10]=1;b[13]=64;b[14]=3;b[24]=2;b[26]=1;for(int i=0;i<6;i++)b[18+i]='0';
        nlohmann::json row={{"dir","gamefiles"},{"filename","demo-r1.z5"},{"type","z5"},{"title","Demonstration"},{"release","1"},{"serial","000000"},{"md5",md5(b)},{"comment","Masterpieces collection fixture."}};
        auto data=nlohmann::json::array({row});auto catalog=parse_online_catalog(data.dump());assert(catalog.size()==1);auto release=catalog[0].releases[0];
        auto second=row;second["filename"]="demo-r99.z5";second["release"]="99";second["serial"]="999999";second["comment"]="Experimental";
        auto older=row;older["type"]="z2";older["filename"]="old.z2";data.push_back(second);data.push_back(older);
        assert(parse_online_catalog(data.dump())[0].releases.front().release==1);
        for(auto name:{"../escape.z5","a/b.z5","a\\b.z5","..z5","%2e%2e.z5","demo.z6","https://bad.z5"}){auto bad=row;bad["filename"]=name;rejects([&]{parse_online_catalog(nlohmann::json::array({bad}).dump());});}
        for(auto key:{"release","serial","md5","title"}){auto bad=row;bad[key]=nullptr;rejects([&]{parse_online_catalog(nlohmann::json::array({bad}).dump());});}
        rejects([&]{parse_online_catalog(nlohmann::json::array({row,row}).dump());});rejects([&]{parse_online_catalog("{}");});rejects([&]{parse_online_catalog(std::string(catalog_limit+1,' '));});
        auto story=install_online_story(release,b,dir/"stories");assert(story.valid&&read_bytes(story.path)==b);
        assert(install_online_story(release,b,dir/"stories").id==story.id); // identical is idempotent
        auto changed=b;changed[80]=1;rejects([&]{install_online_story(release,changed,dir/"stories");});assert(read_bytes(story.path)==b);
        auto collision=release;collision.md5=md5(changed);rejects([&]{install_online_story(collision,changed,dir/"stories");});assert(read_bytes(story.path)==b);
        auto wrong=release;wrong.release=2;rejects([&]{install_online_story(wrong,b,dir/"stories");});
        fs::create_symlink(story.path,dir/"link.z5");wrong=release;wrong.filename="link.z5";rejects([&]{install_online_story(wrong,b,dir);});assert(read_bytes(story.path)==b);
        atomic_text(dir/"keep","before");rejects([&]{atomic_write(dir/"keep",b,false);});assert(read_text(dir/"keep")=="before");
        Paths paths{dir/"config",dir/"data",dir/"stories"};auto text=data.dump();save_online_catalog(paths,{text.begin(),text.end()});rejects([&]{save_online_catalog(paths,{'b','a','d'});});assert(cached_online_catalog(paths).size()==1);
        std::vector<Story>library={inspect_story(story.path)};annotate_online(library,paths);assert(library[0].title=="Demonstration");
        fs::rename(story.path,dir/"stories/renamed.z5");library=scan(dir/"stories");annotate_online(library,paths);assert(library[0].title=="Demonstration");
        // A published digest is insufficient if the bytes are not a story.
        std::vector<uint8_t>bad(80,0);wrong=release;wrong.md5=md5(bad);rejects([&]{install_online_story(wrong,bad,dir/"invalid");});assert(!fs::exists(dir/"invalid"));
        fs::remove_all(dir);std::cout<<"PASS catalogue validation/filter/order, verified install, overwrite/symlink refusal, cache preservation, renamed download identity\n";
    }catch(...){fs::remove_all(dir);throw;}
}
