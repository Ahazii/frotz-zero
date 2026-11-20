// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalog.hpp"
namespace fz {
struct CatalogEntry {
    int version, release;
    const char *serial, *checksum, *title, *edition, *summary, *source;
};
#include "story_catalog.inc"
bool identify_story(Story& story) {
    for (const auto& entry : catalog_entries) {
        if (story.version != entry.version || story.release != entry.release ||
            story.serial != entry.serial || story.checksum != entry.checksum) continue;
        story.title = entry.title;
        story.summary = entry.summary;
        story.edition = entry.edition;
        story.catalog_source = entry.source;
        return true;
    }
    return false;
}
}
