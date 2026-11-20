// SPDX-License-Identifier: GPL-3.0-or-later
#include "online.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#include <stdexcept>
namespace fz {
namespace {
std::string lower(std::string s){for(auto&c:s)c=std::tolower(static_cast<unsigned char>(c));return s;}
bool hex_md5(const std::string&s){return s.size()==32&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool filename_ok(const std::string&s,int version){
    if(s.empty()||s.size()>128||!std::isalnum(static_cast<unsigned char>(s[0])))return false;
    if(s.find("..")!=std::string::npos||s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.")!=std::string::npos)return false;
    return fs::path(s).extension()==".z"+std::to_string(version);
}
std::string field(const nlohmann::json& j,const char*key,size_t limit,bool optional=false){
    if(optional&&!j.contains(key))return {};
    if(!j.contains(key)||!j[key].is_string())throw std::runtime_error(std::string("Catalogue has an invalid ")+key+" field");
    auto value=j[key].get<std::string>();
    if(value.size()>limit||value.find('\0')!=std::string::npos)throw std::runtime_error("Catalogue field is too long or contains a null byte");
    for(char&c:value)if(static_cast<unsigned char>(c)<32&&c!='\n')c=' ';
    return value;
}
bool preferred(const OnlineRelease&r){auto notes=lower(r.notes);return notes.find("masterpieces")!=std::string::npos||notes.find("ltoi")!=std::string::npos;}
void validate_release(const OnlineRelease&r){
    if(r.version<3||r.version>6||r.release<0||r.release>65535||r.serial.size()!=6||!filename_ok(r.filename,r.version)||!hex_md5(r.md5))throw std::runtime_error("Invalid release identity or download filename");
}
struct CurlInit {CurlInit(){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw std::runtime_error("Cannot initialise HTTPS");}~CurlInit(){curl_global_cleanup();}};
struct Sink {std::vector<uint8_t> data;Transfer* progress;size_t limit;bool too_large=false;};
size_t receive(char* bytes,size_t size,size_t count,void* context){
    auto&s=*static_cast<Sink*>(context);
    if(size&&count>SIZE_MAX/size){s.too_large=true;return 0;}
    size_t n=size*count;
    if(n>s.limit-s.data.size()){s.too_large=true;return 0;}
    if(s.progress->cancel)return 0;
    try{s.data.insert(s.data.end(),bytes,bytes+n);}catch(...){return 0;}
    s.progress->bytes=s.data.size();return n;
}
int progress(void* context,curl_off_t total,curl_off_t now,curl_off_t,curl_off_t){
    auto&s=*static_cast<Sink*>(context);
    if(total>0)s.progress->total=static_cast<size_t>(total);
    if(total>static_cast<curl_off_t>(s.limit)||now>static_cast<curl_off_t>(s.limit)){s.too_large=true;return 1;}
    return s.progress->cancel?1:0;
}
}
OnlineCatalog parse_online_catalog(const std::string& text){
    if(text.size()>catalog_limit)throw std::runtime_error("Catalogue exceeds the size limit");
    nlohmann::json data;
    try{data=nlohmann::json::parse(text);}catch(const nlohmann::json::exception&){throw std::runtime_error("The downloaded catalogue is unreadable. Try refreshing it again later.");}
    if(!data.is_array()||data.size()>4096)throw std::runtime_error("Unexpected catalogue format");
    std::map<std::string,std::vector<OnlineRelease>> titles;std::set<std::string> files;
    for(const auto&item:data){
        auto type=field(item,"type",32),dir=field(item,"dir",128);
        if(dir!="gamefiles"||(type!="z3"&&type!="z4"&&type!="z5"&&type!="z6"))continue;
        OnlineRelease r;r.version=type[1]-'0';r.title=field(item,"title",160);r.filename=field(item,"filename",128);
        r.serial=field(item,"serial",6);r.md5=lower(field(item,"md5",32));r.notes=field(item,"comment",4096,true);r.tags=field(item,"tags",128,true);
        auto release=field(item,"release",5);if(release.empty()||release.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid catalogue release number");r.release=std::stoi(release);
        validate_release(r);if(r.title.empty()||!files.insert(r.filename).second)throw std::runtime_error("Empty title or duplicate catalogue filename");
        titles[r.title].push_back(std::move(r));
    }
    OnlineCatalog result;
    for(auto&[title,releases]:titles){
        std::sort(releases.begin(),releases.end(),[](const auto&a,const auto&b){if(preferred(a)!=preferred(b))return preferred(a);if(a.serial!=b.serial)return a.serial>b.serial;if(a.release!=b.release)return a.release>b.release;return a.filename<b.filename;});
        result.push_back({title,std::move(releases)});
    }
    std::sort(result.begin(),result.end(),[](const auto&a,const auto&b){return lower(a.title)<lower(b.title);});
    if(result.empty())throw std::runtime_error("No supported v3-v6 stories found in the catalogue");return result;
}
std::string release_label(const OnlineRelease&r){
    if(r.source==OnlineSource::IFArchive)return r.filename+" / "+std::to_string((r.bytes+1023)/1024)+" KB";
    std::string label="R"+std::to_string(r.release)+" / "+r.serial+" / V"+std::to_string(r.version);
    if(preferred(r))label+=" *";
    else if(!r.tags.empty())label+=" "+r.tags;
    return label;
}
std::string release_notes(const OnlineRelease&r){
    if(r.source==OnlineSource::IFArchive)return r.title+"\n\n"+r.notes+"\n\nFile: "+r.filename+"\nSize: "+std::to_string(r.bytes)+" bytes\n\nFrom the IF Archive Z-code collection. ZIP downloads may contain several stories and supporting files. The catalogue checksum is checked before installation.";
    return r.title+"\n"+release_label(r)+"\n\n"+(r.notes.empty()?"No release notes supplied by the catalogue.":r.notes)+
        (r.tags.empty()?"":"\nTags: "+r.tags)+"\n\nFile: "+r.filename+
        "\n\n* marks a Masterpieces or Lost Treasures release mentioned in the catalogue. A higher release number does not always mean a finished or better version. Check notes for prototypes and sound differences."+
        (r.version==6?"\n\nV6 may need a matching Blorb media pack. Story downloads do not include separate media.":"");
}
std::vector<uint8_t> https_get(const std::string&url,Transfer&status,size_t limit,const fs::path&ca_file){
    if(url.rfind("https://",0)!=0)throw std::runtime_error("Only HTTPS downloads are supported");
    static CurlInit init;
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);
    if(!curl)throw std::runtime_error("Cannot create HTTPS request");
    Sink sink{{},&status,limit};char error[CURL_ERROR_SIZE]={};
    curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl.get(),CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl.get(),CURLOPT_SSL_VERIFYHOST,2L);
    if(!ca_file.empty())curl_easy_setopt(curl.get(),CURLOPT_CAINFO,ca_file.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,10L);
    curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,60L);curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_LIMIT,100L);curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_TIME,15L);
    curl_easy_setopt(curl.get(),CURLOPT_USERAGENT,"Frotz-Zero/" FROTZ_ZERO_VERSION);
    curl_easy_setopt(curl.get(),CURLOPT_ERRORBUFFER,error);curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&sink);
    curl_easy_setopt(curl.get(),CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl.get(),CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl.get(),CURLOPT_XFERINFODATA,&sink);
    auto result=curl_easy_perform(curl.get());long code=0;curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&code);
    if(status.cancel)throw std::runtime_error("Download cancelled. Existing files were preserved.");
    if(sink.too_large)throw std::runtime_error("Download exceeds the size limit");
    if(result!=CURLE_OK)throw std::runtime_error(std::string("Connection failed: ")+(error[0]?error:curl_easy_strerror(result)));
    if(code!=200)throw std::runtime_error("Server returned HTTP "+std::to_string(code)+". Try refreshing the catalogue later.");
    return std::move(sink.data);
}
std::vector<uint8_t> fetch_online(const OnlineRelease*r,Transfer&status,const fs::path&fixture,OnlineSource source){
    if(r)source=r->source;
    if(source==OnlineSource::IFArchive){
        if(r)validate_archive_release(*r);
        auto relative=r?r->download_path:std::string("indexes/Master-Index.xml");auto limit=r?archive_download_limit:archive_catalog_limit;
        if(!fixture.empty()){if(status.cancel)throw std::runtime_error("Download cancelled");auto data=read_bytes(fixture/relative,limit);status.total=data.size();status.bytes=data.size();return data;}
        // Catalogue paths are validated ASCII paths, never arbitrary URLs.
        return https_get("https://ifarchive.org/"+relative,status,limit);
    }
    if(r)validate_release(*r);
    auto relative=r?"gamefiles/"+r->filename:std::string("catalog.json");auto limit=r?download_limit:catalog_limit;
    if(!fixture.empty()){if(status.cancel)throw std::runtime_error("Download cancelled");auto data=read_bytes(fixture/relative,limit);status.total=data.size();status.bytes=data.size();return data;}
    return https_get(std::string(online_url)+relative,status,limit);
}
OnlineCatalog cached_online_catalog(const Paths&p,OnlineSource source){if(source==OnlineSource::IFArchive)return read_archive_cache(p);return parse_online_catalog(read_text(p.data/"online/catalog.json",catalog_limit));}
OnlineCatalog save_online_catalog(const Paths&p,const std::vector<uint8_t>&bytes,OnlineSource source){
    if(source==OnlineSource::IFArchive)return save_archive_cache(p,bytes);
    auto catalog=parse_online_catalog({bytes.begin(),bytes.end()});atomic_write(p.data/"online/catalog.json",bytes);return catalog;
}
Story install_online_story(const OnlineRelease&r,const std::vector<uint8_t>&bytes,const fs::path&folder){
    validate_release(r);
    if(bytes.size()>download_limit||md5(bytes)!=r.md5)throw std::runtime_error("Download checksum does not match the catalogue. Nothing was installed.");
    auto dest=folder/r.filename;auto story=inspect_story_data(dest,bytes);
    if(!story.valid||story.version!=r.version||story.release!=r.release||story.serial!=r.serial)throw std::runtime_error("Story header does not match a playable catalogue release. Nothing was installed. "+story.issue);
    // A few historical development files have a bad VM checksum. Preserve the
    // published bytes only when their full-file digest matches the catalogue.
    if(fs::exists(dest)||fs::is_symlink(dest)){
        if(fs::is_symlink(dest)||!fs::is_regular_file(dest)||read_bytes(dest,download_limit)!=bytes)throw std::runtime_error("A different file already uses this name. Existing file preserved; move it yourself before retrying.");
    }else atomic_write(dest,bytes,false);
    if(story.catalog_source.empty()){story.title=r.title;story.summary=r.notes.empty()?"No synopsis is available for this release. Open details for its catalogue identity.":r.notes;story.edition="Online catalogue";story.catalog_source=online_url;}
    return story;
}
void annotate_online(std::vector<Story>&stories,const Paths&p){
    for(auto&s:stories)if(s.valid&&s.catalog_source.empty())try{
        auto data=nlohmann::json::parse(read_text(p.data/"online/installed"/(s.id+".json"),32768));
        if(field(data,"id",64)!=s.id)continue;
        auto title=field(data,"title",240),notes=field(data,"notes",16384),source=field(data,"source",128);
        if(source!=archive_url&&source!=online_url)continue;
        s.title=title;s.summary=notes;s.catalog_source=source;s.edition="Downloaded story";
    }catch(...){}
    OnlineCatalog catalog;try{catalog=cached_online_catalog(p);}catch(...){}
    std::map<std::string,const OnlineRelease*> releases;
    for(const auto&title:catalog)for(const auto&r:title.releases)releases.emplace(r.md5,&r);
    for(auto&s:stories){
        if(!s.valid||!s.catalog_source.empty())continue;auto found=releases.find(s.md5);if(found==releases.end())continue;
        const auto&r=*found->second;if(s.version!=r.version||s.release!=r.release||s.serial!=r.serial)continue;
        s.title=r.title;s.summary=r.notes.empty()?"No synopsis is available for this release. Open details for its catalogue identity.":r.notes;s.edition="Online catalogue";s.catalog_source=online_url;
    }
    sort_stories(stories);
}
void remember_online_story(const Paths&p,const OnlineRelease&r,const Story&s){
    nlohmann::json data={{"id",s.id},{"title",s.title},{"notes",s.summary},{"source",r.source==OnlineSource::IFArchive?archive_url:online_url}};
    atomic_text(p.data/"online/installed"/(s.id+".json"),data.dump());
}
}
