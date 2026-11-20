// SPDX-License-Identifier: GPL-3.0-or-later
#include "online.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <libxml/xmlreader.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/fs.h>
namespace fz {
namespace {
std::string lower(std::string s){for(auto&c:s)c=std::tolower(static_cast<unsigned char>(c));return s;}
bool story_extension(const fs::path&p){auto e=lower(p.extension().string());return (e.size()==3&&e[0]=='.'&&e[1]=='z'&&e[2]>='3'&&e[2]<='8')||e==".zblorb"||e==".zlb"||e==".blb"||e==".blorb"||e==".dat";}
bool bundle_extension(const fs::path&p){auto e=lower(p.extension().string());return story_extension(p)||e==".png"||e==".jpg"||e==".jpeg"||e==".txt"||e==".md"||e==".pdf";}
bool safe_relative(const std::string&s){
    if(s.empty()||s.size()>240||s[0]=='/'||s.find('\\')!=std::string::npos||s.find(':')!=std::string::npos)return false;
    for(unsigned char c:s)if(c<32||c==127)return false;
    int depth=0;for(const auto&part:fs::path(s)){if(part==".."||part==".")return false;if(++depth>6)return false;}
    return true;
}
std::string tidy(std::string s){
    // Render catalogue Markdown links as readable labels. No HTML is executed.
    for(size_t pos=0;(pos=s.find("](",pos))!=std::string::npos;){auto end=s.find(')',pos+2),start=s.rfind('[',pos);if(end==std::string::npos||start==std::string::npos){pos+=2;continue;}s.erase(pos,end-pos+1);s.erase(start,1);pos=start;}
    for(char&c:s)if(static_cast<unsigned char>(c)<32&&c!='\n')c=' ';
    auto first=s.find_first_not_of(" \n\r\t");if(first==std::string::npos)return {};s.erase(0,first);s.erase(s.find_last_not_of(" \n\r\t")+1);return s;
}
std::string title_for(const std::string&wiki,const std::string&notes,const std::string&filename){
    std::string title=wiki;
    if(title.empty()){
        title=notes.substr(0,notes.find('\n'));auto by=title.find(" by ");if(by==std::string::npos)by=title.find(", by ");
        if(by!=std::string::npos)title.resize(by);
        for(const auto&marker:{", an ",", a ",". Release ",", Release "}){auto pos=title.find(marker);if(pos!=std::string::npos)title.resize(pos);}
        if(title.empty()||title.size()>100)title=fs::path(filename).stem().string();
    }
    std::replace(title.begin(),title.end(),'_',' ');title=tidy(title);while(!title.empty()&&(title.back()==','||title.back()=='.'))title.pop_back();
    if(title.size()>160)title=fs::path(filename).stem().string();return title;
}
OnlineCatalog grouped(std::vector<OnlineRelease> releases){
    std::map<std::string,std::vector<OnlineRelease>> titles;std::set<std::string> seen;
    for(auto&r:releases){validate_archive_release(r);if(!seen.insert(r.download_path).second)throw std::runtime_error("Duplicate archive catalogue path");titles[r.title].push_back(std::move(r));}
    OnlineCatalog out;for(auto&[name,list]:titles)out.push_back({name,std::move(list)});
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return lower(a.title)<lower(b.title);});
    if(out.empty())throw std::runtime_error("No supported Z-code downloads in the archive catalogue");return out;
}
void decorate(Story&s,const OnlineRelease&r,bool multiple){
    if(!s.catalog_source.empty())return;
    s.title=r.title+(multiple?" / "+s.path.stem().string():"");s.summary=r.notes.empty()?"No description supplied by the IF Archive.":r.notes;
    s.edition="IF Archive";s.catalog_source=archive_url;
}
}
void validate_archive_release(const OnlineRelease&r){
    const std::string prefix="if-archive/games/zcode/";
    if(r.source!=OnlineSource::IFArchive||r.download_path.rfind(prefix,0)!=0||!safe_relative(r.download_path)||
       r.download_path.find("/old/")!=std::string::npos||r.download_path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.+/")!=std::string::npos||
       fs::path(r.download_path).filename()!=r.filename||r.filename.size()>160||r.title.empty()||r.title.size()>240||r.notes.size()>16384||
       r.md5.size()!=32||r.md5.find_first_not_of("0123456789abcdef")!=std::string::npos||!r.bytes||r.bytes>archive_download_limit||
       (!story_extension(r.filename)&&lower(fs::path(r.filename).extension().string())!=".zip"))throw std::runtime_error("Invalid IF Archive download identity");
}
OnlineCatalog parse_archive_catalog(const std::string&text){
    if(text.size()>archive_catalog_limit||text.find("<!DOCTYPE")!=std::string::npos||text.find("<!ENTITY")!=std::string::npos)throw std::runtime_error("Unsafe or oversized archive catalogue");
    std::unique_ptr<xmlTextReader,decltype(&xmlFreeTextReader)> reader(xmlReaderForMemory(text.data(),int(text.size()),nullptr,nullptr,XML_PARSE_NONET|XML_PARSE_NOERROR|XML_PARSE_NOWARNING),xmlFreeTextReader);
    if(!reader)throw std::runtime_error("Unreadable archive catalogue");
    std::vector<OnlineRelease> releases;OnlineRelease r;bool in_file=false;int file_depth=0,result;std::string key,wiki;
    while((result=xmlTextReaderRead(reader.get()))==1){
        int type=xmlTextReaderNodeType(reader.get()),depth=xmlTextReaderDepth(reader.get());
        if(depth>16||type==XML_READER_TYPE_DOCUMENT_TYPE||type==XML_READER_TYPE_ENTITY_REFERENCE)throw std::runtime_error("Unexpected archive XML structure");
        auto raw=xmlTextReaderConstLocalName(reader.get());std::string name=raw?reinterpret_cast<const char*>(raw):"";
        if(type==XML_READER_TYPE_ELEMENT&&name=="file") {if(in_file)throw std::runtime_error("Nested archive entry");in_file=true;file_depth=depth;r={};r.source=OnlineSource::IFArchive;key.clear();wiki.clear();continue;}
        if(!in_file)continue;
        if(type==XML_READER_TYPE_END_ELEMENT&&name=="file"&&depth==file_depth){
            in_file=false;auto ext=lower(fs::path(r.filename).extension().string());
            if(r.download_path.rfind("if-archive/games/zcode/",0)!=0||r.download_path.find("/old/")!=std::string::npos||(!story_extension(r.filename)&&ext!=".zip"))continue;
            // Oversized files and non-URL-safe legacy names remain on the website.
            if(!r.bytes||r.bytes>archive_download_limit||r.download_path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.+/")!=std::string::npos)continue;
            r.notes=tidy(r.notes);r.title=title_for(wiki,r.notes,r.filename);validate_archive_release(r);releases.push_back(std::move(r));
            if(releases.size()>8192)throw std::runtime_error("Too many archive titles");continue;
        }
        if(type!=XML_READER_TYPE_ELEMENT)continue;
        bool direct=depth==file_depth+1;
        if(!((direct&&(name=="name"||name=="path"||name=="md5"||name=="size"||name=="description"))||name=="key"||name=="value"))continue;
        xmlChar*value=xmlTextReaderReadString(reader.get());std::string v=value?reinterpret_cast<char*>(value):"";xmlFree(value);
        if(v.size()>16384)throw std::runtime_error("Oversized archive metadata field");
        if(direct&&name=="name")r.filename=v;else if(direct&&name=="path")r.download_path=v;else if(direct&&name=="md5")r.md5=lower(v);
        else if(direct&&name=="size"){if(v.empty()||v.size()>12||v.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid archive file size");r.bytes=std::stoull(v);}
        else if(direct&&name=="description")r.notes=v;else if(name=="key")key=v;else if(name=="value"&&key=="ifwiki")wiki=v;
    }
    if(result<0||in_file)throw std::runtime_error("Truncated or invalid archive catalogue");return grouped(std::move(releases));
}
OnlineCatalog save_archive_cache(const Paths&p,const std::vector<uint8_t>&bytes){
    auto catalog=parse_archive_catalog({bytes.begin(),bytes.end()});nlohmann::json rows=nlohmann::json::array();
    for(const auto&t:catalog)for(const auto&r:t.releases)rows.push_back({{"file",r.filename},{"path",r.download_path},{"title",r.title},{"notes",r.notes},{"md5",r.md5},{"bytes",r.bytes}});
    auto data=rows.dump();if(data.size()>catalog_limit)throw std::runtime_error("Archive cache exceeds size limit");atomic_text(p.data/"online/ifarchive.json",data);return catalog;
}
OnlineCatalog read_archive_cache(const Paths&p){
    auto data=nlohmann::json::parse(read_text(p.data/"online/ifarchive.json",catalog_limit));if(!data.is_array()||data.size()>8192)throw std::runtime_error("Invalid archive cache");
    std::vector<OnlineRelease> releases;for(const auto&item:data){OnlineRelease r;r.source=OnlineSource::IFArchive;r.filename=item.at("file");r.download_path=item.at("path");r.title=item.at("title");r.notes=item.at("notes");r.md5=item.at("md5");r.bytes=item.at("bytes");releases.push_back(std::move(r));}return grouped(std::move(releases));
}
std::vector<Story> install_archive_release(const OnlineRelease&r,const std::vector<uint8_t>&bytes,const fs::path&folder){
    validate_archive_release(r);if(bytes.size()!=r.bytes||md5(bytes)!=r.md5)throw std::runtime_error("Archive download checksum or size mismatch. Nothing was installed.");
    if(lower(fs::path(r.filename).extension().string())!=".zip"){
        auto dest=folder/r.filename;auto story=inspect_story_data(dest,bytes);if(!story.valid)throw std::runtime_error("This download is not a supported Z-machine story: "+story.issue);
        if(fs::exists(dest)||fs::is_symlink(dest)){if(fs::is_symlink(dest)||!fs::is_regular_file(dest)||read_bytes(dest)!=bytes)throw std::runtime_error("A different file already uses this name. Existing file preserved.");}
        else atomic_write(dest,bytes,false);decorate(story,r,false);return {story};
    }
    std::unique_ptr<archive,decltype(&archive_read_free)> zip(archive_read_new(),archive_read_free);
    if(!zip)throw std::runtime_error("Cannot open ZIP");archive_read_support_format_zip(zip.get());archive_read_support_filter_none(zip.get());
    if(archive_read_open_memory(zip.get(),bytes.data(),bytes.size())!=ARCHIVE_OK)throw std::runtime_error("Unreadable ZIP download");
    std::map<fs::path,std::vector<uint8_t>> files;std::set<std::string> names;archive_entry*entry=nullptr;size_t total=0,count=0;int status;
    while((status=archive_read_next_header(zip.get(),&entry))==ARCHIVE_OK){
        const char*raw=archive_entry_pathname(entry);std::string name=raw?raw:"";
        if(++count>512||!safe_relative(name)||archive_entry_symlink(entry)||archive_entry_hardlink(entry)||archive_entry_is_encrypted(entry)>0)throw std::runtime_error("ZIP contains unsafe paths, links, encryption or too many files");
        if(archive_entry_filetype(entry)==AE_IFDIR)continue;
        if(archive_entry_filetype(entry)!=AE_IFREG||!names.insert(lower(name)).second)throw std::runtime_error("ZIP contains special or duplicate files");
        auto size=archive_entry_size(entry);if(size<0||size>32*1024*1024||uint64_t(size)+total>64*1024*1024)throw std::runtime_error("ZIP exceeds the unpacked size limit");total+=size;
        if(!bundle_extension(name)){if(archive_read_data_skip(zip.get())!=ARCHIVE_OK)throw std::runtime_error("Unreadable ZIP entry");continue;}
        std::vector<uint8_t>data(static_cast<size_t>(size));size_t pos=0;while(pos<data.size()){auto n=archive_read_data(zip.get(),data.data()+pos,data.size()-pos);if(n<=0)throw std::runtime_error("Truncated ZIP entry");pos+=n;}
        uint8_t extra;if(archive_read_data(zip.get(),&extra,1)!=0)throw std::runtime_error("ZIP entry length or checksum mismatch");files.emplace(fs::path(name),std::move(data));
    }
    if(status!=ARCHIVE_EOF)throw std::runtime_error("Invalid ZIP download");
    // Validate every playable payload before writing. Install the whole bundle
    // into a unique directory, then atomically make it visible to the library.
    size_t playable=0;for(const auto&[name,data]:files)if(story_extension(name)&&inspect_story_data(name,data).valid)++playable;
    if(!playable)throw std::runtime_error("This ZIP has no supported Z-machine stories. Nothing was installed.");
    auto parent=folder/"IFArchive";if(fs::is_symlink(parent))throw std::runtime_error("The bundle folder is a link; existing files preserved");fs::create_directories(parent);auto dest=parent/(safe_name(fs::path(r.filename).stem().string())+"-"+r.md5.substr(0,12));
    if(fs::exists(dest)||fs::is_symlink(dest)){
        if(fs::is_symlink(dest)||!fs::is_directory(dest))throw std::runtime_error("Existing bundle path is not a directory");
        for(const auto&[name,data]:files){auto path=dest/name;for(auto current=path;current!=dest;current=current.parent_path())if(fs::is_symlink(current))throw std::runtime_error("Existing bundle contains a link; preserved");if(read_bytes(path)!=data)throw std::runtime_error("Existing bundle differs; preserved");}
    }else{
        auto temp=parent/(".pending-"+timestamp());if(!fs::create_directory(temp))throw std::runtime_error("Cannot create a fresh bundle directory");
        try{for(const auto&[name,data]:files)atomic_write(temp/name,data,false);if(syscall(SYS_renameat2,AT_FDCWD,temp.c_str(),AT_FDCWD,dest.c_str(),RENAME_NOREPLACE)!=0)throw std::runtime_error("Cannot install bundle without replacing an existing path; retry download");int fd=open(parent.c_str(),O_DIRECTORY);if(fd>=0){fsync(fd);close(fd);}}
        catch(...){std::error_code ec;fs::remove_all(temp,ec);throw;}
    }
    auto stories=scan(dest);stories.erase(std::remove_if(stories.begin(),stories.end(),[](const auto&s){return !s.valid;}),stories.end());
    for(auto&s:stories)decorate(s,r,stories.size()>1);return stories;
}
}
