// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/utility/importer/safari_importer.h"

#include <map>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/data_url.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace {

// Function to recursively read Bookmarks out of Safari plist.
//
//   - `bookmark_folder`: The dictionary containing a folder to parse.
//   - `parent_path_elements`: Path elements up to this point.
//   - `is_in_toolbar`: True if this folder is in the toolbar.
//   - `out_bookmarks`: The Bookmark element array to write into.
void RecursiveReadBookmarksFolder(
    NSDictionary* bookmark_folder,
    const std::vector<std::u16string>& parent_path_elements,
    bool is_in_toolbar,
    const std::u16string& toolbar_name,
    std::vector<ImportedBookmarkEntry>* out_bookmarks) {
  DCHECK(bookmark_folder);

  NSString* type = bookmark_folder[@"WebBookmarkType"];
  NSString* title = bookmark_folder[@"Title"];

  // Are we the dictionary that contains all other bookmarks?
  // We need to know this so we don't add it to the path.
  bool is_top_level_bookmarks_container =
      bookmark_folder[@"WebBookmarkFileVersion"] != nil;

  // We're expecting a list of bookmarks here, if that isn't what we got, fail.
  if (!is_top_level_bookmarks_container) {
    // Top level containers sometimes don't have title attributes.
    if (![type isEqualToString:@"WebBookmarkTypeList"] || !title) {
      NOTREACHED() << "Type=("
                   << (type ? base::SysNSStringToUTF8(type) : "Null type")
                   << ") Title=("
                   << (title ? base::SysNSStringToUTF8(title) : "Null title")
                   << ")";
      return;
    }
  }

  NSArray* elements = bookmark_folder[@"Children"];
  if (!elements && (!parent_path_elements.empty() || !is_in_toolbar) &&
      ![title isEqualToString:@"BookmarksMenu"]) {
    // This is an empty folder, so add it explicitly.  Note that the condition
    // above prevents either the toolbar folder or the bookmarks menu from being
    // added if either is empty.  Note also that all non-empty folders are added
    // implicitly when their children are added.
    ImportedBookmarkEntry entry;
    // Safari doesn't specify a creation time for the folder.
    entry.creation_time = base::Time::Now();
    entry.title = base::SysNSStringToUTF16(title);
    entry.path = parent_path_elements;
    entry.in_toolbar = is_in_toolbar;
    entry.is_folder = true;

    out_bookmarks->push_back(entry);
    return;
  }

  std::vector<std::u16string> path_elements(parent_path_elements);
  // Create a folder for the toolbar, but not for the bookmarks menu.
  if (path_elements.empty() && [title isEqualToString:@"BookmarksBar"]) {
    is_in_toolbar = true;
    path_elements.push_back(toolbar_name);
  } else if (!is_top_level_bookmarks_container &&
             !(path_elements.empty() &&
               [title isEqualToString:@"BookmarksMenu"])) {
    if (title) {
      path_elements.push_back(base::SysNSStringToUTF16(title));
    }
  }

  // Iterate over individual bookmarks.
  for (NSDictionary* bookmark in elements) {
    NSString* element_type = bookmark[@"WebBookmarkType"];
    if (!element_type) {
      continue;
    }

    // If this is a folder, recurse.
    if ([element_type isEqualToString:@"WebBookmarkTypeList"]) {
      RecursiveReadBookmarksFolder(bookmark, path_elements, is_in_toolbar,
                                   toolbar_name, out_bookmarks);
    }

    // If we didn't see a bookmark folder, then we're expecting a bookmark
    // item.  If that's not what we got then ignore it.
    if (![element_type isEqualToString:@"WebBookmarkTypeLeaf"]) {
      continue;
    }

    NSString* element_url = bookmark[@"URLString"];
    NSString* element_title = bookmark[@"URIDictionary"][@"title"];

    if (!element_url || !element_title) {
      continue;
    }

    // Output Bookmark.
    ImportedBookmarkEntry entry;
    // Safari doesn't specify a creation time for the bookmark.
    entry.creation_time = base::Time::Now();
    entry.title = base::SysNSStringToUTF16(element_title);
    entry.url = GURL(base::SysNSStringToUTF8(element_url));
    entry.path = path_elements;
    entry.in_toolbar = is_in_toolbar;

    out_bookmarks->push_back(entry);
  }
}

}  // namespace

SafariImporter::SafariImporter(const base::FilePath& library_dir)
    : library_dir_(library_dir) {
}

SafariImporter::~SafariImporter() = default;

void SafariImporter::StartImport(const importer::SourceProfile& source_profile,
                                 uint16_t items,
                                 ImporterBridge* bridge) {
  bridge_ = bridge;
  // The order here is important!
  bridge_->NotifyStarted();

  // In keeping with import on other platforms (and for other browsers), we
  // don't import the home page (since it may lead to a useless homepage); see
  // crbug.com/25603.
  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportBookmarks();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }

  bridge_->NotifyEnded();
}

void SafariImporter::ImportBookmarks() {
  std::u16string toolbar_name =
      bridge_->GetLocalizedString(IDS_BOOKMARK_BAR_FOLDER_NAME);
  std::vector<ImportedBookmarkEntry> bookmarks;
  ParseBookmarks(toolbar_name, &bookmarks);

  // Write bookmarks into profile.
  if (!bookmarks.empty() && !cancelled()) {
    const std::u16string& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_SAFARI);
    bridge_->AddBookmarks(bookmarks, first_folder_name);
  }

  // Import favicons.
  sql::Database db;
  if (!OpenDatabase(&db))
    return;

  FaviconMap favicon_map;
  ImportFaviconURLs(&db, &favicon_map);
  // Write favicons into profile.
  if (!favicon_map.empty() && !cancelled()) {
    favicon_base::FaviconUsageDataList favicons;
    LoadFaviconData(&db, favicon_map, &favicons);
    bridge_->SetFavicons(favicons);
  }
}

bool SafariImporter::OpenDatabase(sql::Database* db) {
  // Construct ~/Library/Safari/WebpageIcons.db path.
  NSString* library_dir = base::SysUTF8ToNSString(library_dir_.value());
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* favicons_db_path = [safari_dir
      stringByAppendingPathComponent:@"WebpageIcons.db"];

  const char* db_path = [favicons_db_path fileSystemRepresentation];
  return db->Open(base::FilePath(db_path));
}

void SafariImporter::ImportFaviconURLs(sql::Database* db,
                                       FaviconMap* favicon_map) {
  const char query[] = "SELECT iconID, url FROM PageURL;";
  sql::Statement s(db->GetUniqueStatement(query));

  while (s.Step() && !cancelled()) {
    int64_t icon_id = s.ColumnInt64(0);
    GURL url = GURL(s.ColumnString(1));
    (*favicon_map)[icon_id].insert(url);
  }
}

void SafariImporter::LoadFaviconData(
    sql::Database* db,
    const FaviconMap& favicon_map,
    favicon_base::FaviconUsageDataList* favicons) {
  const char query[] = "SELECT i.url, d.data "
                       "FROM IconInfo i JOIN IconData d "
                       "ON i.iconID = d.iconID "
                       "WHERE i.iconID = ?;";
  sql::Statement s(db->GetUniqueStatement(query));

  for (FaviconMap::const_iterator i = favicon_map.begin();
       i != favicon_map.end(); ++i) {
    s.Reset(true);
    s.BindInt64(0, i->first);
    if (s.Step()) {
      favicon_base::FaviconUsageData usage;

      usage.favicon_url = GURL(s.ColumnString(0));
      if (!usage.favicon_url.is_valid())
        continue;  // Don't bother importing favicons with invalid URLs.

      std::vector<unsigned char> data;
      s.ColumnBlobAsVector(1, &data);
      if (data.empty())
        continue;  // Data definitely invalid.

      if (!importer::ReencodeFavicon(&data[0], data.size(), &usage.png_data))
        continue;  // Unable to decode.

      usage.urls = i->second;
      favicons->push_back(usage);
    }
  }
}

void SafariImporter::ParseBookmarks(
    const std::u16string& toolbar_name,
    std::vector<ImportedBookmarkEntry>* bookmarks) {
  DCHECK(bookmarks);

  // Construct ~/Library/Safari/Bookmarks.plist path
  NSString* library_dir = base::SysUTF8ToNSString(library_dir_.value());
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* bookmarks_plist = [safari_dir
    stringByAppendingPathComponent:@"Bookmarks.plist"];

  // Load the plist file.
  NSDictionary* bookmarks_dict = [NSDictionary
      dictionaryWithContentsOfFile:bookmarks_plist];
  if (!bookmarks_dict)
    return;

  // Recursively read in bookmarks.
  std::vector<std::u16string> parent_path_elements;
  RecursiveReadBookmarksFolder(bookmarks_dict, parent_path_elements, false,
                               toolbar_name, bookmarks);
}
