// SPDX-License-Identifier: GPL-2.0-or-later
#include "model.hpp"
#include "catalog.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <linux/fs.h>
#include <set>
namespace fz {
static std::string env(const char* key,const std::string& fallback) { const char* s=getenv(key);return s&&*s?s:fallback; }
Paths paths() {
    auto h=env("HOME",".");
    return {fs::path(env("XDG_CONFIG_HOME",h+"/.config"))/"frotz-zero",fs::path(env("XDG_DATA_HOME",h+"/.local/share"))/"frotz-zero",fs::path(h)/"Games/FrotzZero/stories"};
}
std::vector<uint8_t> read_bytes(const fs::path& p,size_t limit) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("Cannot read "+p.string());
    auto n=f.tellg();if(n<0||uint64_t(n)>limit)throw std::runtime_error("File too large: "+p.string());
    std::vector<uint8_t> b(static_cast<size_t>(n));f.seekg(0);
    if(n&&!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("Incomplete read: "+p.string());return b;
}
std::string read_text(const fs::path& p,size_t limit){auto b=read_bytes(p,limit);return {b.begin(),b.end()};}
static std::string digest(const std::vector<uint8_t>& b,const EVP_MD* algorithm) {
    unsigned char hash[EVP_MAX_MD_SIZE];unsigned int n=0;
    if(!EVP_Digest(b.data(),b.size(),hash,&n,algorithm,nullptr))throw std::runtime_error("Content digest failed");
    std::ostringstream out;for(unsigned i=0;i<n;i++)out<<std::hex<<std::setw(2)<<std::setfill('0')<<int(hash[i]);return out.str();
}
std::string sha256(const std::vector<uint8_t>& b){return digest(b,EVP_sha256());}
std::string md5(const std::vector<uint8_t>& b){return digest(b,EVP_md5());}
std::string file_hash(const fs::path& p){return sha256(read_bytes(p));}
std::string json_string(const std::string& s) {
    std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c);else o<<c;}o<<'"';return o.str();
}
void atomic_write(const fs::path& p,const std::vector<uint8_t>& b,bool replace) {
    fs::create_directories(p.parent_path());auto temp=p;temp+=".tmp-"+std::to_string(getpid())+"-"+timestamp();
    int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)throw std::runtime_error("Cannot create "+temp.string()+": "+strerror(errno));
    size_t at=0;bool ok=true;while(at<b.size()){ssize_t n=write(fd,b.data()+at,b.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=n;}
    if(fsync(fd))ok=false;if(close(fd))ok=false;
    if(!ok){fs::remove(temp);throw std::runtime_error("Write failed; previous data preserved (check free space and permissions)");}
    if(replace){std::error_code ec;fs::rename(temp,p,ec);if(ec){fs::remove(temp);throw std::runtime_error("Cannot replace "+p.string()+": "+ec.message());}}
    else {
        // link() creates the destination atomically and never replaces a file or
        // symlink, including one created after the caller's existence check.
        int result=link(temp.c_str(),p.c_str()),error=errno;fs::remove(temp);
        if(result)throw std::runtime_error("Cannot create "+p.string()+": "+strerror(error)+". Existing files were preserved.");
    }
    int dir=open(p.parent_path().c_str(),O_DIRECTORY|O_RDONLY);if(dir>=0){fsync(dir);close(dir);}
}
void atomic_text(const fs::path& p,const std::string& s){atomic_write(p,{s.begin(),s.end()});}
Settings read_settings(const Paths& p){
    Settings s;s.stories=p.stories;std::ifstream f(p.config/"settings.conf");std::string line;
    while(std::getline(f,line)){auto n=line.find('=');if(n==std::string::npos)continue;auto k=line.substr(0,n),v=line.substr(n+1);
        try{if(k=="stories")s.stories=v;else if(k=="theme"&&(v=="dark"||v=="light"))s.theme=v;else if(k=="text_size")s.text_size=std::clamp(std::stoi(v),12,24);else if(k=="volume")s.volume=std::clamp(std::stoi(v),0,100);else if(k=="mute")s.mute=v=="1";else if(k=="recent")s.recent=v;}catch(...){}
    }return s;
}
void write_settings(const Paths&p,const Settings&s){
    if(s.stories.string().find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Folder name contains a line break");
    atomic_text(p.config/"settings.conf","stories="+s.stories.string()+"\ntheme="+s.theme+"\ntext_size="+std::to_string(s.text_size)+"\nvolume="+std::to_string(s.volume)+"\nmute="+(s.mute?"1":"0")+"\nrecent="+s.recent+"\n");
}
static uint16_t be16(const uint8_t*p){return (p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t*p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(p[2]<<8)|p[3];}
struct Blorb {std::vector<uint8_t> code;bool pictures=false,sound=false;std::vector<uint8_t> ifhd;size_t cover_start=0,cover_length=0;};
static Blorb blorb(const std::vector<uint8_t>&b){
    Blorb r;std::map<uint32_t,uint32_t> images;int64_t cover=-1;bool cover_seen=false,cover_invalid=false;if(b.size()<12||memcmp(b.data(),"FORM",4)||memcmp(b.data()+8,"IFRS",4))throw std::runtime_error("Invalid Blorb header");
    size_t end=uint64_t(be32(b.data()+4))+8;if(end>b.size()||end<12)throw std::runtime_error("Truncated Blorb");
    for(size_t i=12;i+8<=end;){auto n=be32(b.data()+i+4);if(uint64_t(i)+8+n>end)throw std::runtime_error("Truncated Blorb chunk");
        std::string t(reinterpret_cast<const char*>(b.data()+i),4);
        if(t=="ZCOD"){if(!r.code.empty())throw std::runtime_error("Multiple ZCOD chunks");r.code.assign(b.begin()+i+8,b.begin()+i+8+n);}
        if(t=="IFhd")r.ifhd.assign(b.begin()+i+8,b.begin()+i+8+n);
        if(t=="Fspc"){if(n!=4||cover_seen)cover_invalid=true;else cover=be32(b.data()+i+8);cover_seen=true;}
        if(t=="RIdx"){if(n<4)throw std::runtime_error("Bad resource index");auto count=be32(b.data()+i+8);if(count>(n-4)/12)throw std::runtime_error("Bad resource index length");
            for(uint32_t j=0;j<count;j++){auto p=b.data()+i+12+j*12;auto off=be32(p+8);if(uint64_t(off)+8>end)throw std::runtime_error("Resource offset outside Blorb");r.pictures|=!memcmp(p,"Pict",4);r.sound|=!memcmp(p,"Snd ",4);if(!memcmp(p,"Pict",4))images[be32(p+4)]=off;}}
        i+=8+n+(n&1);
    }
    if(!cover_invalid&&cover>=0&&images.count(uint32_t(cover))){size_t pos=images[uint32_t(cover)];auto n=be32(b.data()+pos+4);if(uint64_t(pos)+8+n<=end&&n<=8*1024*1024&&(!memcmp(b.data()+pos,"PNG ",4)||!memcmp(b.data()+pos,"JPEG",4))){r.cover_start=pos+8;r.cover_length=n;}}
    return r;
}
static std::vector<fs::path> cover_paths(const fs::path&story){
    std::vector<fs::path> out;
    for(const auto&suffix:{".cover.png",".cover.jpg",".cover.jpeg",".png",".jpg",".jpeg"}){
        out.push_back(story.parent_path()/(story.stem().string()+suffix));
    }return out;
}
std::vector<uint8_t> read_story_cover(const Story&s){
    for(const auto&p:cover_paths(s.path)){std::error_code ec;
        if(!fs::is_symlink(p,ec)&&fs::is_regular_file(p,ec))try{return read_bytes(p,8*1024*1024);}catch(...){}
    }
    if(!s.resource.empty())try{auto bytes=read_bytes(s.resource);auto info=blorb(bytes);if(info.cover_length)return {bytes.begin()+info.cover_start,bytes.begin()+info.cover_start+info.cover_length};}catch(...){}
    return {};
}
static void note(Story&s,const std::string&t){if(!s.issue.empty())s.issue+="; ";s.issue+=t;}
Story inspect_story_data(const fs::path&p,const std::vector<uint8_t>&data){
    Story s;s.path=fs::absolute(p);s.title=p.filename().string();
    auto add_note=[&](const std::string&t){note(s,t);};
    try{
        auto b=data;if(b.size()>=4&&!memcmp(b.data(),"FORM",4)){auto br=blorb(b);b=std::move(br.code);s.resource=s.path;s.pictures=br.pictures;s.sound=br.sound;}
        if(b.size()<64)throw std::runtime_error("Header shorter than 64 bytes");s.bytes=b.size();s.version=b[0];
        if(s.version<3||s.version>8)throw std::runtime_error("Supported Z-machine versions are 3 to 8");
        s.release=be16(b.data()+2);s.serial=std::string(reinterpret_cast<char*>(b.data()+18),6);
        for(char&c:s.serial)if(c<32||c>126)c='?';
        std::ostringstream cs;cs<<std::hex<<std::setw(4)<<std::setfill('0')<<be16(b.data()+28);s.checksum=cs.str();
        s.id=sha256(b);s.md5=md5(b);size_t declared=size_t(be16(b.data()+26))*(s.version<=3?2:s.version<=5?4:8);
        if(declared&&declared>b.size())throw std::runtime_error("Declared file length exceeds available bytes");
        if(!declared)add_note("Header has no declared length");
        auto dynamic=be16(b.data()+14);if(dynamic<64||dynamic>b.size())throw std::runtime_error("Invalid dynamic memory boundary");
        for(int at:{8,10,12,24})if(be16(b.data()+at)>=b.size())throw std::runtime_error("Header table outside story");
        if(size_t(be16(b.data()+12))+480>dynamic)throw std::runtime_error("Globals exceed dynamic memory");
        if(declared){uint16_t sum=0;for(size_t i=64;i<declared;i++)sum+=b[i];if(sum!=be16(b.data()+28))add_note("Checksum mismatch (original preserved)");if(declared<b.size())add_note("Trailing bytes beyond declared length");}
        s.valid=true;
        s.unusual=s.title.find("gamma")!=std::string::npos||s.title.find("test")!=std::string::npos||s.title.find("generic")!=std::string::npos||s.serial.find_first_not_of("0123456789")!=std::string::npos;
        if(s.issue.find("Checksum mismatch")==std::string::npos&&identify_story(s)) {
            s.unusual=s.edition!="Adventure"&&s.edition!="CC0 demonstration";
            if(s.unusual)add_note("Catalogue: "+s.edition);
        } else if(s.unusual)add_note("Uncatalogued or unusual identity; filename retained");
    }catch(const std::exception&e){s.valid=false;add_note(e.what());}return s;
}
static std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});return s;}
static bool media_extension(const fs::path&p){auto e=lower(p.extension().string());return e==".blb"||e==".blorb"||e==".zblorb"||e==".zlb";}
struct Media {fs::path path;Blorb info;std::string code_id,problem;};
static Media inspect_media(const fs::path&p){
    Media m;m.path=fs::absolute(p);
    try{m.info=blorb(read_bytes(p));if(!m.info.ifhd.empty()&&m.info.ifhd.size()<10)throw std::runtime_error("Incomplete media identity");if(!m.info.code.empty()){m.code_id=sha256(m.info.code);m.info.code.clear();}}
    catch(const std::exception&e){m.problem=e.what();}return m;
}
static void attach_media(Story&s,const std::vector<Media>&media){
    if(!s.valid)return;
    if(s.resource.empty()){
        const Media* chosen=nullptr;int best=0;bool ambiguous=false;
        for(const auto&m:media){
            if(m.path.parent_path()!=s.path.parent_path())continue;
            bool same=lower(m.path.stem().string())==lower(s.path.stem().string());
            if(!m.problem.empty()){if(same)note(s,"Invalid companion ignored: "+m.path.filename().string());continue;}
            const auto&b=m.info;bool identified=!m.code_id.empty()||!b.ifhd.empty();
            bool match=m.code_id.empty()||m.code_id==s.id;
            if(!b.ifhd.empty()){
                std::ostringstream ck;ck<<std::hex<<std::setw(4)<<std::setfill('0')<<be16(b.ifhd.data()+8);
                match=match&&be16(b.ifhd.data())==s.release&&std::string(reinterpret_cast<const char*>(b.ifhd.data()+2),6)==s.serial&&ck.str()==s.checksum;
            }
            if(!match){if(same)note(s,"Companion resource release mismatch; ignored");continue;}
            if((!identified&&!same)||(!b.pictures&&!b.sound))continue;
            int score=identified?(same?3:2):1;
            if(score>best){best=score;chosen=&m;ambiguous=false;}else if(score==best)ambiguous=true;
        }
        if(ambiguous)note(s,"Multiple matching media packs; use one matching companion with the story's base filename");
        else if(chosen){s.resource=chosen->path;s.pictures=chosen->info.pictures;s.sound=chosen->info.sound;if(best==1)note(s,"Companion has no release identity; verify media provenance");}
    }
    if(s.version==6&&!s.pictures)note(s,"No indexed pictures: provide matching .blb/.blorb; graphics-dependent play is unverified");
}
Story inspect_story(const fs::path&p){
    Story s;
    try{s=inspect_story_data(p,read_bytes(p));}catch(const std::exception&e){s.path=fs::absolute(p);s.title=p.filename().string();s.issue=e.what();return s;}
    std::vector<Media> media;std::error_code ec;
    if(s.valid&&s.resource.empty())for(auto it=fs::directory_iterator(s.path.parent_path(),fs::directory_options::skip_permission_denied,ec);!ec&&it!=fs::directory_iterator();it.increment(ec))if(!it->is_symlink(ec)&&it->is_regular_file(ec)&&media_extension(it->path()))media.push_back(inspect_media(it->path()));
    attach_media(s,media);return s;
}
static std::vector<Story> scan_entries(const fs::path& dir,bool deduplicate){
    std::vector<Story> out;std::vector<fs::path> candidates;std::vector<Media> media;std::error_code ec;
    if(!fs::is_directory(dir,ec))return out;
    fs::recursive_directory_iterator it(dir,fs::directory_options::skip_permission_denied,ec),end;
    for(;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}if(it.depth()>6){it.disable_recursion_pending();continue;}if(it->is_symlink(ec)||!it->is_regular_file(ec))continue;
        auto e=lower(it->path().extension().string());
        bool candidate=e==".z3"||e==".z4"||e==".z5"||e==".z6"||e==".z7"||e==".z8"||e==".zblorb"||e==".zlb"||e==".dat"||e.empty();
        if(media_extension(it->path())){media.push_back(inspect_media(it->path()));candidate=candidate||!media.back().code_id.empty();}
        if(candidate)candidates.push_back(it->path());}
    std::sort(candidates.begin(),candidates.end());std::map<std::string,size_t> seen;
    for(const auto&p:candidates){
        Story s;try{s=inspect_story_data(p,read_bytes(p));}catch(const std::exception&e){s.path=fs::absolute(p);s.title=p.filename().string();s.issue=e.what();}
        attach_media(s,media);auto found=seen.find(s.id);
        if(!deduplicate||s.id.empty()||found==seen.end()){if(!s.id.empty())seen[s.id]=out.size();out.push_back(s);}
        else if((s.pictures||s.sound)&&!(out[found->second].pictures||out[found->second].sound))out[found->second]=s;
    }
    sort_stories(out);return out;
}
std::vector<Story> scan(const fs::path&dir){return scan_entries(dir,true);}
static void deletion_file(const fs::path&root,const fs::path&file){
    auto relative=file.lexically_relative(root);
    if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||fs::canonical(file)!=file||!fs::is_regular_file(file))
        throw std::runtime_error("Deletion stopped: file is outside the library, linked, or no longer a regular file.");
}
StoryDeletion plan_story_deletion(const fs::path&library,const Story&story){
    StoryDeletion plan{fs::canonical(library),story,{}};auto selected=fs::canonical(story.path);
    deletion_file(plan.root,selected);
    auto entries=scan_entries(plan.root,false);std::set<fs::path> candidates,protected_files;bool found=false;
    for(const auto&s:entries){
        bool same=story.id.empty()?s.path==selected:s.id==story.id;
        auto&files=same?candidates:protected_files;
        files.insert(s.path);for(const auto&p:cover_paths(s.path))files.insert(p);
        if(!s.resource.empty()){files.insert(s.resource);for(const auto&p:cover_paths(s.resource))files.insert(p);}
        found|=same;
    }
    if(!found)throw std::runtime_error("This title changed or is no longer in the library. Refresh and try again.");
    for(const auto&p:candidates){
        std::error_code ec;if(protected_files.count(p)||!fs::exists(p,ec))continue;
        // A linked sidecar is deliberately retained, even if its target is local.
        if(fs::is_symlink(p,ec))continue;
        deletion_file(plan.root,p);plan.files.emplace(p,file_hash(p));
    }
    if(plan.files.empty())throw std::runtime_error("No exclusive story files can be deleted. Shared files have been kept.");
    return plan;
}
size_t delete_story_files(const StoryDeletion&plan,const Paths&paths){
    // A newly added story may now share a companion; reconfirm instead of using
    // a stale list. Content hashes also protect files replaced since confirmation.
    auto fresh=plan_story_deletion(plan.root,plan.story);
    if(fresh.files!=plan.files)throw std::runtime_error("The library changed. Nothing was deleted; review the file list again.");
    struct Lock {int fd=-1;~Lock(){if(fd>=0){flock(fd,LOCK_UN);close(fd);}}} lock;
    if(!plan.story.id.empty()){
        auto folder=paths.data/"stories"/plan.story.id;
        if(fs::is_directory(folder)){
            lock.fd=open((folder/"session.lock").c_str(),O_CREAT|O_RDWR|O_NOFOLLOW,0600);
            if(lock.fd<0||flock(lock.fd,LOCK_EX|LOCK_NB))throw std::runtime_error("This title is open in another session. Close it before deleting.");
        }
    }
    for(const auto&[file,hash]:plan.files){deletion_file(plan.root,file);if(file_hash(file)!=hash||access(file.parent_path().c_str(),W_OK|X_OK))throw std::runtime_error("Nothing was deleted: a file changed or its folder is not writable.");}
    size_t removed=0;
    for(const auto&[file,hash]:plan.files){
        try{deletion_file(plan.root,file);if(file_hash(file)!=hash)throw std::runtime_error("File changed");if(!fs::remove(file))throw std::runtime_error("File no longer exists");++removed;}
        catch(const std::exception&e){throw std::runtime_error(std::to_string(removed)+" files deleted before an error: "+e.what()+". Saves were kept. Refresh the library before trying again.");}
    }
    return removed;
}
void sort_stories(std::vector<Story>&out){std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){auto x=lower(a.title),y=lower(b.title);return x==y?a.path.filename()<b.path.filename():x<y;});}
void inventory(const std::vector<Story>&all,const fs::path&file){
    std::ostringstream o;o<<"[\n";bool first=true;
    for(const auto&s:all){
        if(!first)o<<",\n";first=false;
        o<<" {\"filename\":"<<json_string(s.path.filename().string())<<",\"title\":"<<json_string(s.title)
         <<",\"summary\":"<<json_string(s.summary)<<",\"edition\":"<<json_string(s.edition)
         <<",\"version\":"<<s.version<<",\"release\":"<<s.release<<",\"serial\":"<<json_string(s.serial)
         <<",\"checksum\":"<<json_string(s.checksum)<<",\"sha256\":"<<json_string(s.id)<<",\"md5\":"<<json_string(s.md5)
         <<",\"resource\":"<<json_string(s.resource.filename().string())<<",\"pictures\":"<<(s.pictures?"true":"false")
         <<",\"sound\":"<<(s.sound?"true":"false")<<",\"bytes\":"<<s.bytes
         <<",\"valid\":"<<(s.valid?"true":"false")<<",\"findings\":"<<json_string(s.issue)<<"}";
    }
    o<<"\n]\n";atomic_text(file,o.str());
}
bool portable_save_matches(const fs::path&p,const Story&s){try{auto b=read_bytes(p);if(b.size()<12||memcmp(b.data(),"FORM",4)||memcmp(b.data()+8,"IFZS",4)||uint64_t(be32(b.data()+4))+8!=b.size())return false;bool identity=false,memory=false,stack=false;size_t i=12;for(;i+8<=b.size();){auto n=be32(b.data()+i+4);if(uint64_t(i)+8+n+(n&1)>b.size())return false;auto q=b.data()+i+8;if(!memcmp(b.data()+i,"IFhd",4)){if(identity||n!=13)return false;std::ostringstream ck;ck<<std::hex<<std::setw(4)<<std::setfill('0')<<be16(q+8);identity=be16(q)==s.release&&std::string(reinterpret_cast<char*>(q+2),6)==s.serial&&ck.str()==s.checksum;if(!identity)return false;}if(!memcmp(b.data()+i,"CMem",4)||!memcmp(b.data()+i,"UMem",4)){if(memory||!n)return false;memory=true;}if(!memcmp(b.data()+i,"Stks",4)){if(stack||n<8)return false;stack=true;}i+=8+n+(n&1);}return i==b.size()&&identity&&memory&&stack;}catch(...){}return false;}
std::string safe_name(const std::string&s){std::string r;for(unsigned char c:s)if(std::isalnum(c)||c=='-'||c=='_'||c==' ')r+=c;while(!r.empty()&&r.back()==' ')r.pop_back();if(r.size()>48)r.resize(48);return r;}
std::string timestamp(){auto t=std::chrono::system_clock::now();auto micros=std::chrono::duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count();return std::to_string(micros);}
static bool validate_generation(const fs::path&p,const std::string&id){
    try{auto text=read_text(p/"manifest",65536);std::istringstream in(text);std::string schema,identity;std::getline(in,schema);std::getline(in,identity);if(schema!="FZCP-1"||identity!=id)return false;
        std::string name,hash;std::map<std::string,bool> found;while(in>>name>>hash){if(name!="vm.bin"&&name!="screen.bin"&&name!="frontend.txt")return false;if(found[name]||hash.size()!=64||fs::is_symlink(p/name))return false;if(file_hash(p/name)!=hash)return false;found[name]=true;}return found.size()==3;}catch(...){return false;}
}
Generation find_generation(const fs::path&base,const std::string&id){
    Generation g;std::vector<fs::path> dirs;std::error_code ec;if(!fs::is_directory(base,ec))return g;
    for(auto&e:fs::directory_iterator(base,ec))if(e.is_directory()&&e.path().filename().string().rfind("gen-",0)==0)dirs.push_back(e.path());std::sort(dirs.rbegin(),dirs.rend());
    for(auto&p:dirs){if(validate_generation(p,id)){g.path=p;return g;}g.problem="Newest checkpoint was incomplete or corrupt; using previous valid generation if available.";}return g;
}
void commit_generation(const fs::path&base,const std::string&id,const std::map<std::string,std::vector<uint8_t>>&parts){
    fs::create_directories(base);auto temp=base/("pending-"+timestamp()),dest=base/("gen-"+timestamp());fs::create_directory(temp);
    try{std::string manifest="FZCP-1\n"+id+"\n";for(auto&[name,b]:parts){if(name.find_first_of("/\\")!=std::string::npos||name.find("..")!=std::string::npos)throw std::runtime_error("Invalid checkpoint component");atomic_write(temp/name,b);manifest+=name+" "+sha256(b)+"\n";}atomic_text(temp/"manifest",manifest);if(!validate_generation(temp,id))throw std::runtime_error("Incomplete checkpoint");fs::rename(temp,dest);int fd=open(base.c_str(),O_DIRECTORY);if(fd>=0){fsync(fd);close(fd);}
        std::vector<fs::path> good;for(auto&e:fs::directory_iterator(base))if(e.path().filename().string().rfind("gen-",0)==0&&validate_generation(e.path(),id))good.push_back(e.path());std::sort(good.rbegin(),good.rend());for(size_t i=2;i<good.size();i++)fs::remove_all(good[i]);
    }catch(...){std::error_code ec;fs::remove_all(temp,ec);throw;}
}
static fs::path named_save_path(const fs::path&folder,const fs::path&path){
    auto root=fs::canonical(folder),file=fs::absolute(path).lexically_normal();auto ext=file.extension();
    if(file.parent_path()!=root||fs::is_symlink(file)||fs::canonical(file)!=file||
       (ext!=".qzl"&&ext!=".fzsave")||(!fs::is_regular_file(file)&&!(ext==".fzsave"&&fs::is_directory(file))))
        throw std::runtime_error("Only named saves in this story's saves folder can be managed. Automatic Continue is protected.");
    return file;
}
static std::string save_fingerprint(const fs::path&file){
    if(fs::is_regular_file(file))return file_hash(file);
    std::map<std::string,std::string> parts;size_t count=0;
    for(auto it=fs::recursive_directory_iterator(file);it!=fs::recursive_directory_iterator();++it){
        if(++count>64||it.depth()>3||it->is_symlink()||fs::canonical(it->path())!=it->path())
            throw std::runtime_error("Checkpoint contains links or an unexpected directory layout; no files were changed.");
        if(it->is_regular_file())parts[it->path().lexically_relative(file).generic_string()]=file_hash(it->path());
        else if(it->is_directory())parts[it->path().lexically_relative(file).generic_string()]="directory";
        else throw std::runtime_error("Checkpoint contains a special file; no files were changed.");
    }
    std::string manifest;for(const auto&[name,hash]:parts)manifest+=json_string(name)+":"+hash+"\n";
    return sha256({manifest.begin(),manifest.end()});
}
std::vector<NamedSave> named_saves(const fs::path&folder,const Story&story){
    std::vector<NamedSave> out;std::error_code ec;if(!fs::is_directory(folder,ec))return out;
    auto root=fs::canonical(folder);
    for(const auto&e:fs::directory_iterator(root)){
        auto ext=e.path().extension();if(e.is_symlink()||(ext!=".qzl"&&ext!=".fzsave"))continue;
        if(!e.is_regular_file()&&!(ext==".fzsave"&&e.is_directory()))continue;
        NamedSave save;save.path=e.path();save.checkpoint=ext==".fzsave";
        try{named_save_path(root,save.path);save.fingerprint=save_fingerprint(save.path);
            if(save.checkpoint){auto generation=find_generation(save.path,story.id);save.valid=bool(generation);save.issue=generation.problem;if(!save.valid)save.issue="No complete checkpoint generation remains.";}
            else{save.valid=portable_save_matches(save.path,story);if(!save.valid)save.issue="This save is incomplete or belongs to a different story release.";}
        }catch(const std::exception&e){save.issue=e.what();}
        out.push_back(std::move(save));
    }
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){std::error_code ea,eb;auto ta=fs::last_write_time(a.path,ea),tb=fs::last_write_time(b.path,eb);return !ea&&!eb&&ta!=tb?ta>tb:a.path.filename()<b.path.filename();});return out;
}
static fs::path unchanged_named_save(const fs::path&folder,const NamedSave&save){
    auto file=named_save_path(folder,save.path);
    if(save.fingerprint.empty()||save_fingerprint(file)!=save.fingerprint)
        throw std::runtime_error("This save changed or could not be checked. Nothing was changed; reopen the save list.");
    return file;
}
fs::path rename_named_save(const fs::path&folder,const NamedSave&save,const std::string&name){
    if(name.empty()||safe_name(name)!=name)throw std::runtime_error("Use 1-48 letters, numbers, spaces, hyphens or underscores. The save type stays the same.");
    auto source=unchanged_named_save(folder,save),dest=source.parent_path()/(name+source.extension().string());
    if(dest==source)return source;
    // The kernel refuses any existing destination atomically, including one
    // created between a UI check and this operation. Works for directories too.
    if(syscall(SYS_renameat2,AT_FDCWD,source.c_str(),AT_FDCWD,dest.c_str(),RENAME_NOREPLACE)){
        int error=errno;
        if(error==EINVAL||error==ENOSYS||error==EOPNOTSUPP){
            if(fs::is_regular_file(source)){
                // Same-filesystem hard link gives files an atomic, exclusive
                // destination on filesystems without renameat2 (e.g. WSL DrvFS).
                if(link(source.c_str(),dest.c_str())==0){
                    if(unlink(source.c_str()))throw std::runtime_error("The new name was created, but the old name could not be removed. Both copies were kept.");
                    return dest;
                }error=errno;
            }else{
                // Reserve an empty destination exclusively. POSIX rename can
                // replace this empty directory, but refuses files/nonempty dirs.
                std::error_code ec;
                if(fs::create_directory(dest,ec)){
                    fs::rename(source,dest,ec);if(!ec)return dest;
                    error=ec.value();std::error_code cleanup;fs::remove(dest,cleanup);
                }else error=ec?ec.value():EEXIST;
            }
        }
        throw std::runtime_error("Save was not renamed: "+std::string(strerror(error))+". Existing saves were kept.");
    }
    return dest;
}
void delete_named_save(const fs::path&folder,const NamedSave&save){
    auto file=unchanged_named_save(folder,save);
    // A checkpoint is an explicitly selected .fzsave directory directly under
    // this story's saves folder. Every descendant was checked above; links and
    // special files are refused. The sibling automatic resume is never a target.
    std::error_code ec;fs::remove_all(file,ec);
    if(ec)throw std::runtime_error("Deletion could not finish: "+ec.message()+". Refresh this list; automatic Continue and other saves were not targeted.");
}
}
