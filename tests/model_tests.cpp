// SPDX-License-Identifier: GPL-2.0-or-later
#include "model.hpp"
#include "catalog.hpp"
#include "view.hpp"
#include <cassert>
#include <iostream>
#include <sys/resource.h>
#include <csignal>
#include <algorithm>
using namespace fz;
int main(){auto dir=fs::temp_directory_path()/("fz-test-"+timestamp());fs::create_directories(dir);try{
    Story known;known.title="arbitrary-renamed-file.z3";known.version=3;known.release=88;known.serial="840726";known.checksum="a129";
    assert(identify_story(known)&&known.title=="Zork I: The Great Underground Empire"&&!known.summary.empty());
    known.title="unknown.z3";known.checksum="a128";assert(!identify_story(known)&&known.title=="unknown.z3");
    auto fitted=make_viewport(640,400,0.425,0,0);assert(fitted.target.x==24&&fitted.target.w==272);
    assert(fitted.story_point(100,85)==std::make_pair(178,200));
    auto readable=make_viewport(640,400,1.0,24,50);assert(readable.story_point(100,85)==std::make_pair(124,135));
    assert(readable.screen_point(124,135)==std::make_pair(100,85));
    auto fit=make_viewport(640,400,ViewMode::Fit,2,100,100);assert(fit.source.w==640&&fit.source.h==400&&fit.source.x==0&&fit.target.w==272&&fit.target.h==170);
    auto fill=make_viewport(640,400,ViewMode::Fill,2,100,100);assert(fill.target.w==320&&fill.target.h==170&&fill.story_point(160,85)==std::make_pair(320,200));
    auto wide=make_viewport(640,400,ViewMode::Width,2,0,100);assert(wide.source.w==640&&wide.source.h==340&&wide.source.y==60&&wide.target.w==320);
    std::vector<uint8_t>b(1024);b[0]=3;b[3]=1;b[4]=3;b[6]=3;b[8]=2;b[10]=1;b[12]=0;b[13]=64;b[14]=3;b[24]=2;b[26]=2;for(int i=0;i<6;i++)b[18+i]='0';atomic_write(dir/"alpha.z3",b);
    auto first=inspect_story(dir/"alpha.z3");assert(first.valid&&first.version==3);fs::rename(dir/"alpha.z3",dir/"renamed.z3");assert(inspect_story(dir/"renamed.z3").id==first.id);
    atomic_write(dir/"duplicate.z3",b);assert(scan(dir).size()==1);b[3]=2;atomic_write(dir/"release2.z3",b);assert(scan(dir).size()==2);assert(inspect_story(dir/"release2.z3").id!=first.id);
    b.resize(63);atomic_write(dir/"truncated.z5",b);assert(!inspect_story(dir/"truncated.z5").valid);
    std::map<std::string,std::vector<uint8_t>>parts={{"vm.bin",{1,2,3}},{"screen.bin",{4,5,6}},{"frontend.txt",{7,8,9}}};auto base=dir/"resume";commit_generation(base,first.id,parts);auto good=find_generation(base,first.id);assert(good);parts["vm.bin"]={4,5,6};commit_generation(base,first.id,parts);auto newer=find_generation(base,first.id);assert(newer&&newer.path!=good.path);atomic_text(newer.path/"vm.bin","truncated");auto fallback=find_generation(base,first.id);assert(fallback&&fallback.path==good.path&&!fallback.problem.empty());assert(!find_generation(base,"wrong-story"));
    assert(safe_name("../../my save")=="my save");assert(json_string("a\nb") == "\"a\\u000ab\"");
    // A real failed write must leave the previous file intact (disk-full analogue).
    atomic_text(dir/"retained","before");rlimit old_limit{};getrlimit(RLIMIT_FSIZE,&old_limit);auto limited=old_limit;limited.rlim_cur=16;auto old_handler=signal(SIGXFSZ,SIG_IGN);setrlimit(RLIMIT_FSIZE,&limited);bool failed=false;try{atomic_write(dir/"retained",std::vector<uint8_t>(1024,1));}catch(...){failed=true;}setrlimit(RLIMIT_FSIZE,&old_limit);signal(SIGXFSZ,old_handler);assert(failed&&read_text(dir/"retained")=="before");
    Paths p{dir/"config",dir/"data",dir/"stories"};Settings s;s.stories=dir;s.theme="light";s.mute=true;s.volume=40;s.text_size=16;write_settings(p,s);auto saved=read_settings(p);assert(saved.mute&&saved.volume==40&&saved.theme=="light"&&saved.text_size==16&&saved.stories==dir);
    auto saves=dir/"named-saves";fs::create_directory(saves);
    atomic_text(saves/"first.qzl","damaged but manageable");atomic_text(saves/"taken.qzl","existing save");
    auto entry=[&](const std::string&name){auto list=named_saves(saves,first);auto it=std::find_if(list.begin(),list.end(),[&](const auto&e){return e.path.filename()==name;});assert(it!=list.end());return *it;};
    auto named=entry("first.qzl");assert(!named.valid&&!named.fingerprint.empty());
    bool blocked=false;try{rename_named_save(saves,named,"taken");}catch(...){blocked=true;}assert(blocked&&read_text(saves/"taken.qzl")=="existing save");
    assert(rename_named_save(saves,named,"Renamed save")==saves/"Renamed save.qzl");assert(read_text(saves/"Renamed save.qzl")=="damaged but manageable");
    named=entry("Renamed save.qzl");atomic_text(named.path,"new bytes");blocked=false;try{delete_named_save(saves,named);}catch(...){blocked=true;}assert(blocked&&fs::exists(named.path));
    named=entry("Renamed save.qzl");blocked=false;try{rename_named_save(saves,named,"../escape");}catch(...){blocked=true;}assert(blocked);
    delete_named_save(saves,named);assert(!fs::exists(named.path)&&fs::exists(saves/"taken.qzl"));
    commit_generation(saves/"chapter.fzsave",first.id,parts);auto cp=entry("chapter.fzsave");assert(cp.valid&&cp.checkpoint);
    auto renamed=rename_named_save(saves,cp,"Chapter two");assert(find_generation(renamed,first.id));
    delete_named_save(saves,entry("Chapter two.fzsave"));assert(!fs::exists(renamed)&&find_generation(base,first.id));
    auto automatic=cp;automatic.path=base;blocked=false;try{delete_named_save(saves,automatic);}catch(...){blocked=true;}assert(blocked&&find_generation(base,first.id));
    fs::create_directory(saves/"linked.fzsave");fs::create_directory_symlink(base,saves/"linked.fzsave/outside");auto linked=entry("linked.fzsave");assert(!linked.valid&&linked.fingerprint.empty());
    blocked=false;try{delete_named_save(saves,linked);}catch(...){blocked=true;}assert(blocked&&find_generation(base,first.id));
    fs::create_symlink(saves/"taken.qzl",saves/"alias.qzl");assert(named_saves(saves,first).size()==2);
    fs::remove_all(dir);std::cout<<"PASS header validation, content identity, rename/dedup, release separation, checkpoint corruption fallback, settings persistence\n";
}catch(...){fs::remove_all(dir);throw;}}
