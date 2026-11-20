// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.hpp"
#include <cassert>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
using namespace fz;
void be32(std::vector<uint8_t>&b,unsigned n){for(int i=3;i>=0;i--)b.push_back(n>>(i*8));}
void chunk(std::vector<uint8_t>&b,const std::string&kind,const std::vector<uint8_t>&data){b.insert(b.end(),kind.begin(),kind.end());be32(b,data.size());b.insert(b.end(),data.begin(),data.end());if(data.size()%2)b.push_back(0);}
std::vector<uint8_t> media(const std::vector<uint8_t>&story,bool identity=true,bool front=false){
    std::vector<uint8_t>body={'I','F','R','S'},ifhd;
    if(identity){ifhd={story[2],story[3]};ifhd.insert(ifhd.end(),story.begin()+18,story.begin()+24);ifhd.insert(ifhd.end(),story.begin()+28,story.begin()+30);ifhd.resize(13);chunk(body,"IFhd",ifhd);}
    if(front)chunk(body,"Fspc",{0,0,0,1});
    std::vector<uint8_t>index;be32(index,1);index.insert(index.end(),{'P','i','c','t'});be32(index,1);be32(index,8+body.size()+24);
    chunk(body,"RIdx",index);chunk(body,"PNG ",{0,1,2,3}); // structural resource, never decoded as an image
    std::vector<uint8_t>result={'F','O','R','M'};be32(result,body.size());result.insert(result.end(),body.begin(),body.end());return result;
}
void deletion_tests(const fs::path&base,const std::vector<uint8_t>&b){
    auto dir=base/"delete";fs::create_directory(dir);Paths paths{base/"config",base/"data",dir};
    atomic_write(dir/"chosen.z6",b);atomic_write(dir/"copy.z6",b);atomic_write(dir/"chosen.blb",media(b));atomic_text(dir/"chosen.cover.png","original cover");
    auto selected=inspect_story(dir/"chosen.z6");auto saves=paths.data/"stories"/selected.id;
    atomic_text(saves/"saves/precious.qzl","precious save");atomic_text(saves/"resume/retained","checkpoint");
    auto other=b;other[3]=2;atomic_write(dir/"sequel.z6",other);atomic_text(dir/"manual.pdf","manual retained");
    auto plan=plan_story_deletion(dir,selected);assert(plan.files.size()==4);
    // Refuse changed content before unlinking any duplicate or companion.
    atomic_text(dir/"chosen.cover.png","changed cover");bool refused=false;
    try{delete_story_files(plan,paths);}catch(...){refused=true;}
    assert(refused&&fs::exists(dir/"copy.z6")&&fs::exists(dir/"chosen.z6"));
    plan=plan_story_deletion(dir,selected);
    int fd=open((saves/"session.lock").c_str(),O_CREAT|O_RDWR,0600);assert(fd>=0&&!flock(fd,LOCK_EX|LOCK_NB));refused=false;
    try{delete_story_files(plan,paths);}catch(...){refused=true;}
    assert(refused&&fs::exists(dir/"chosen.blb"));flock(fd,LOCK_UN);close(fd);
    assert(delete_story_files(plan,paths)==4&&scan(dir).size()==1);
    assert(read_text(saves/"saves/precious.qzl")=="precious save"&&read_text(saves/"resume/retained")=="checkpoint");
    assert(read_bytes(dir/"sequel.z6")==other&&read_text(dir/"manual.pdf")=="manual retained");
    // Different content IDs can share the same IFhd media identity. Preserve it.
    atomic_write(dir/"chosen.z6",b);auto variant=b;variant.back()=1;atomic_write(dir/"variant.z6",variant);
    atomic_write(dir/"shared.blb",media(b));selected=inspect_story(dir/"chosen.z6");
    assert(selected.resource==dir/"shared.blb"&&inspect_story(dir/"variant.z6").pictures);
    atomic_text(base/"outside.png","outside");fs::create_symlink(base/"outside.png",dir/"chosen.png");
    plan=plan_story_deletion(dir,selected);assert(plan.files.size()==1&&delete_story_files(plan,paths)==1);
    assert(fs::exists(dir/"shared.blb")&&inspect_story(dir/"variant.z6").pictures&&read_text(base/"outside.png")=="outside");
    // Revalidate sharing that appears after the confirmation list was built.
    fs::remove(dir/"variant.z6");atomic_write(dir/"chosen.z6",b);plan=plan_story_deletion(dir,selected);assert(plan.files.size()==2);
    atomic_write(dir/"variant.z6",variant);refused=false;try{delete_story_files(plan,paths);}catch(...){refused=true;}
    assert(refused&&fs::exists(dir/"chosen.z6")&&fs::exists(dir/"shared.blb"));
    auto outside=selected;outside.path=dir/"sequel.z6";refused=false;try{plan_story_deletion(base/"config",outside);}catch(...){refused=true;}assert(refused);
    // Embedded Blorb and raw file are the same title and both disappear.
    fs::remove(dir/"variant.z6");fs::rename(dir/"shared.blb",dir/"chosen.blb");auto blob=media(b);chunk(blob,"ZCOD",b);auto n=blob.size()-8;for(int i=0;i<4;i++)blob[4+i]=n>>(24-8*i);atomic_write(dir/"embedded.blb",blob);
    plan=plan_story_deletion(dir,selected);assert(plan.files.size()==3);assert(delete_story_files(plan,paths)==3&&scan(dir).size()==1);
}
int main(){auto dir=fs::temp_directory_path()/("fz-media-"+timestamp());fs::create_directories(dir);try{
    std::vector<uint8_t>b(1024);b[0]=6;b[3]=1;b[4]=3;b[6]=3;b[8]=2;b[10]=1;b[13]=64;b[14]=3;b[24]=2;b[27]=128;for(int i=0;i<6;i++)b[18+i]='0';
    auto p=dir/"release-r1.z6";atomic_write(p,b);auto original=inspect_story(p);assert(original.valid&&!original.pictures);
    atomic_write(dir/"Adventure.BLB",media(b));auto matched=inspect_story(p);assert(matched.pictures&&matched.resource.filename()=="Adventure.BLB");assert(scan(dir).size()==1&&scan(dir)[0].pictures);
    auto different=b;different[3]=2;atomic_write(dir/"other.z6",different);assert(!inspect_story(dir/"other.z6").pictures);
    atomic_write(dir/"duplicate.blb",media(b));assert(!inspect_story(p).pictures); // ambiguous packs are never guessed
    atomic_write(dir/"release-r1.blb",media(b));assert(inspect_story(p).resource.filename()=="release-r1.blb");
    fs::remove(dir/"Adventure.BLB");fs::remove(dir/"duplicate.blb");atomic_write(dir/"release-r1.blb",media(different));auto mismatch=inspect_story(p);assert(mismatch.valid&&!mismatch.pictures&&mismatch.issue.find("mismatch")!=std::string::npos);
    atomic_text(dir/"release-r1.blb","truncated");assert(inspect_story(p).valid&&!inspect_story(p).pictures);assert(scan(dir).size()==2);
    atomic_write(dir/"release-r1.blb",media(b,false));assert(inspect_story(p).pictures&&inspect_story(p).issue.find("no release identity")!=std::string::npos);
    fs::rename(dir/"release-r1.blb",dir/"unidentified.blb");assert(!inspect_story(p).pictures);
    assert(inspect_story(p).id==original.id);
    fs::remove(dir/"unidentified.blb");atomic_write(dir/"release-r1.blb",media(b,true,true));
    assert(read_story_cover(inspect_story(p))==std::vector<uint8_t>({0,1,2,3}));
    atomic_write(dir/"release-r1.cover.png",{9,8,7});assert(read_story_cover(inspect_story(p))==std::vector<uint8_t>({9,8,7}));
    fs::remove(dir/"release-r1.cover.png");fs::create_symlink(dir/"release-r1.blb",dir/"release-r1.png");assert(read_story_cover(inspect_story(p))==std::vector<uint8_t>({0,1,2,3}));
    auto malformed=media(b,true,true);chunk(malformed,"Fspc",{1});auto size=malformed.size()-8;for(int i=0;i<4;i++)malformed[4+i]=size>>(24-8*i);atomic_write(dir/"release-r1.blb",malformed);assert(inspect_story(p).valid&&read_story_cover(inspect_story(p)).empty());
    deletion_tests(dir,b);
    fs::remove_all(dir);std::cout<<"PASS media/cover matching and deletion: duplicates, shared media, saves, content changes, active sessions, symlinks and embedded stories\n";
}catch(...){fs::remove_all(dir);throw;}}
