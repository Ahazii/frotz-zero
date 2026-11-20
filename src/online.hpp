// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.hpp"
#include <atomic>
namespace fz {
inline constexpr const char* online_url="https://eblong.com/infocom/";
inline constexpr const char* archive_url="https://ifarchive.org/indexes/if-archive/games/zcode/";
inline constexpr size_t catalog_limit=4*1024*1024,download_limit=8*1024*1024;
inline constexpr size_t archive_catalog_limit=32*1024*1024,archive_download_limit=32*1024*1024;
enum class OnlineSource { Infocom, IFArchive };
struct OnlineRelease {
    std::string filename,title,serial,md5,notes,tags;
    int version=0,release=0;
    OnlineSource source=OnlineSource::Infocom;
    std::string download_path;
    size_t bytes=0;
};
struct OnlineTitle {std::string title;std::vector<OnlineRelease> releases;};
using OnlineCatalog=std::vector<OnlineTitle>;
struct Transfer {
    std::atomic<bool> cancel{false};
    std::atomic<size_t> bytes{0},total{0};
};
OnlineCatalog parse_online_catalog(const std::string&);
std::string release_label(const OnlineRelease&);
std::string release_notes(const OnlineRelease&);
std::vector<uint8_t> https_get(const std::string& url,Transfer&,size_t limit,const fs::path& ca_file={});
// fixture is only set by the explicit deterministic UI test driver.
std::vector<uint8_t> fetch_online(const OnlineRelease* release,Transfer&,const fs::path& fixture={},OnlineSource source=OnlineSource::Infocom);
OnlineCatalog cached_online_catalog(const Paths&,OnlineSource source=OnlineSource::Infocom);
OnlineCatalog save_online_catalog(const Paths&,const std::vector<uint8_t>&,OnlineSource source=OnlineSource::Infocom);
Story install_online_story(const OnlineRelease&,const std::vector<uint8_t>&,const fs::path& folder);
OnlineCatalog parse_archive_catalog(const std::string&);
OnlineCatalog read_archive_cache(const Paths&);
OnlineCatalog save_archive_cache(const Paths&,const std::vector<uint8_t>&);
void validate_archive_release(const OnlineRelease&);
std::vector<Story> install_archive_release(const OnlineRelease&,const std::vector<uint8_t>&,const fs::path& folder);
void remember_online_story(const Paths&,const OnlineRelease&,const Story&);
void annotate_online(std::vector<Story>&,const Paths&);
}
