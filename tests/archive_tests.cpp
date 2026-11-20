// SPDX-License-Identifier: GPL-3.0-or-later
#include "online.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <cassert>
#include <iostream>
using namespace fz;
int rejection=0;
template<class F>void rejects(F f){++rejection;bool failed=false;try{f();}catch(...){failed=true;}if(!failed)std::cerr<<"Rejection check "<<rejection<<" unexpectedly accepted\n";assert(failed);}
std::vector<uint8_t> zipped(const std::string&name,const std::vector<uint8_t>&data,bool link=false){
    std::vector<uint8_t>out(65536);size_t used=0;auto a=archive_write_new();archive_write_set_format_zip(a);assert(archive_write_open_memory(a,out.data(),out.size(),&used)==ARCHIVE_OK);
    auto e=archive_entry_new();archive_entry_set_pathname(e,name.c_str());archive_entry_set_filetype(e,link?AE_IFLNK:AE_IFREG);archive_entry_set_perm(e,0600);archive_entry_set_size(e,link?0:data.size());if(link)archive_entry_set_symlink(e,"outside");
    assert(archive_write_header(a,e)==ARCHIVE_OK);if(!link)assert(archive_write_data(a,data.data(),data.size())==int(data.size()));archive_entry_free(e);assert(archive_write_close(a)==ARCHIVE_OK);archive_write_free(a);out.resize(used);return out;
}
int main(int argc,char**argv){
    if(argc==3&&std::string(argv[1])=="catalog"){auto c=parse_archive_catalog(read_text(argv[2],archive_catalog_limit));size_t n=0;for(auto&t:c)n+=t.releases.size();std::cout<<c.size()<<" titles / "<<n<<" files\n";return 0;}
    if(argc==5&&std::string(argv[1])=="install"){auto c=parse_archive_catalog(read_text(argv[2],archive_catalog_limit));for(auto&t:c)for(auto&r:t.releases)if(r.filename==fs::path(argv[3]).filename()){auto s=install_archive_release(r,read_bytes(argv[3]),argv[4]);std::cout<<s.size()<<" installed stories\n";return 0;}return 2;}
    auto dir=fs::temp_directory_path()/("fz-archive-"+timestamp());fs::create_directories(dir);try{
        std::vector<uint8_t>b(1024);b[0]=8;b[3]=1;b[4]=3;b[6]=3;b[8]=2;b[10]=1;b[13]=64;b[14]=3;b[24]=2;b[27]=128;for(int i=0;i<6;i++)b[18+i]='0';
        auto entry=[&](std::string path){return "<file><name>demo.z8</name><path>"+path+"</path><size>1024</size><md5>"+md5(b)+"</md5><description>Demo by Example. A [short](https://example.org) adventure.</description><metadata><item><key>ifwiki</key><value>Demo &amp; Test</value></item></metadata></file>";};
        auto xml="<ifarchive>"+entry("if-archive/games/zcode/demo.z8")+entry("if-archive/games/zcode/old/demo.z8")+"</ifarchive>";
        auto c=parse_archive_catalog(xml);assert(c.size()==1&&c[0].title=="Demo & Test"&&c[0].releases[0].notes.find("[short]")==std::string::npos);auto r=c[0].releases[0];
        rejects([&]{parse_archive_catalog("<!DOCTYPE a [<!ENTITY x SYSTEM 'file:///etc/passwd'>]>"+xml);});rejects([&]{parse_archive_catalog(xml.substr(0,xml.size()-20));});rejects([&]{parse_archive_catalog("<ifarchive>"+entry("if-archive/games/zcode/../demo.z8")+"</ifarchive>");});
        Paths p{dir/"config",dir/"data",dir/"stories"};save_archive_cache(p,{xml.begin(),xml.end()});assert(read_archive_cache(p).size()==1);rejects([&]{save_archive_cache(p,{'b','a','d'});});assert(read_archive_cache(p).size()==1);
        auto stories=install_archive_release(r,b,p.stories);assert(stories.size()==1&&stories[0].version==8);remember_online_story(p,r,stories[0]);auto scanned=scan(p.stories);annotate_online(scanned,p);assert(scanned[0].title=="Demo & Test");
        assert(install_archive_release(r,b,p.stories)[0].id==stories[0].id);auto bad=b;bad[100]=1;rejects([&]{install_archive_release(r,bad,p.stories);});auto changed=r;changed.md5=md5(bad);rejects([&]{install_archive_release(changed,bad,p.stories);});assert(read_bytes(p.stories/r.filename)==b);
        auto zip=zipped("demo/demo.z8",b);r.filename="demo.zip";r.download_path="if-archive/games/zcode/demo.zip";r.bytes=zip.size();r.md5=md5(zip);auto bundle=install_archive_release(r,zip,p.stories);assert(bundle.size()==1&&bundle[0].version==8&&bundle[0].path.filename()=="demo.z8");assert(install_archive_release(r,zip,p.stories).size()==1);
        for(auto name:{"../outside.z8","/absolute.z8","demo/../../escape.z8","C:/escape.z8","..\\escape.z8"}){auto z=zipped(name,b);r.bytes=z.size();r.md5=md5(z);rejects([&]{install_archive_release(r,z,p.stories);});}
        auto z=zipped("link.z8",b,true);r.bytes=z.size();r.md5=md5(z);rejects([&]{install_archive_release(r,z,p.stories);});
        z=zipped("readme.txt",{'n','o'});r.bytes=z.size();r.md5=md5(z);rejects([&]{install_archive_release(r,z,p.stories);});assert(!fs::exists(dir/"outside.z8"));
        fs::remove_all(dir);std::cout<<"PASS archive XML, entities, cache preservation, V8, identity metadata, checksum/overwrite checks, ZIP extraction/path/link rejection\n";
    }catch(...){fs::remove_all(dir);throw;}
}
