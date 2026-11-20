// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <map>
namespace fz {
namespace fs = std::filesystem;
struct Story {
    fs::path path, resource;
    std::string title, id, md5, serial, checksum, issue;
    std::string summary="No verified synopsis is available for this release. The original filename and header details remain available.", edition, catalog_source;
    int version=0, release=0;
    size_t bytes=0;
    bool valid=false, pictures=false, sound=false, unusual=false;
};
struct Paths { fs::path config, data, stories; };
struct Settings {
    fs::path stories;
    std::string theme="dark", recent;
    int text_size=12, volume=70;
    bool mute=false;
};
Paths paths();
Settings read_settings(const Paths&);
void write_settings(const Paths&,const Settings&);
std::vector<uint8_t> read_bytes(const fs::path&,size_t limit=32*1024*1024);
std::string sha256(const std::vector<uint8_t>&);
std::string md5(const std::vector<uint8_t>&);
std::string file_hash(const fs::path&);
std::string json_string(const std::string&);
void atomic_write(const fs::path&,const std::vector<uint8_t>&,bool replace=true);
void atomic_text(const fs::path&,const std::string&);
std::string read_text(const fs::path&,size_t limit=1024*1024);
Story inspect_story(const fs::path&);
// Inspect downloaded bytes before writing them. Does not search for companions.
Story inspect_story_data(const fs::path&,const std::vector<uint8_t>&);
// Local same-stem image or a Blorb Fspc resource; empty means use original fallback art.
std::vector<uint8_t> read_story_cover(const Story&);
std::vector<Story> scan(const fs::path&);
struct StoryDeletion { fs::path root; Story story; std::map<fs::path,std::string> files; };
StoryDeletion plan_story_deletion(const fs::path&library,const Story&);
// Revalidate the confirmed plan, refuse active sessions, and unlink files only.
// Never removes saves, checkpoints, metadata or directories.
size_t delete_story_files(const StoryDeletion&,const Paths&);
void sort_stories(std::vector<Story>&);
void inventory(const std::vector<Story>&,const fs::path&);
bool portable_save_matches(const fs::path&,const Story&);
std::string safe_name(const std::string&);
std::string timestamp();
struct Generation { fs::path path; std::string problem; explicit operator bool()const{return !path.empty();} };
Generation find_generation(const fs::path&,const std::string& identity);
void commit_generation(const fs::path&,const std::string& identity,const std::map<std::string,std::vector<uint8_t>>&);
struct NamedSave { fs::path path; bool checkpoint=false,valid=false; std::string issue,fingerprint; };
std::vector<NamedSave> named_saves(const fs::path&folder,const Story&);
fs::path rename_named_save(const fs::path&folder,const NamedSave&,const std::string&name);
void delete_named_save(const fs::path&folder,const NamedSave&);
}
