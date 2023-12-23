#ifndef STAR_IMAGE_METADATA_DATABASE_HPP
#define STAR_IMAGE_METADATA_DATABASE_HPP

#include "StarRect.hpp"
#include "StarMap.hpp"
#include "StarString.hpp"
#include "StarThread.hpp"
#include "StarListener.hpp"
#include "StarAssetPath.hpp"

namespace Star {

STAR_CLASS(ImageMetadataDatabase);

// Caches image size, image spaces, and nonEmptyRegion completely until a
// reload, does not expire cached values in a TTL based way like Assets,
// because they are expensive to compute and cheap to keep around.
class ImageMetadataDatabase {
public:
  ImageMetadataDatabase();
  Vec2U imageSize(AssetPath const& path) const;
  List<Vec2I> imageSpaces(AssetPath const& path, Vec2F position, float fillLimit, bool flip) const;
  RectU nonEmptyRegion(AssetPath const& path) const;
  void cleanup(bool triggered = false);

private:
  // Removes image processing directives that don't affect image spaces /
  // non-empty regions.
  static AssetPath filterProcessing(AssetPath const& path);

  Vec2U calculateImageSize(AssetPath const& path) const;

  // Path, position, fillLimit, and flip
  typedef tuple<AssetPath, Vec2I, float, bool> SpacesEntry;

  mutable Mutex m_mutex;
  mutable HashMap<AssetPath, Vec2U> m_sizeCache;
  mutable HashMap<SpacesEntry, List<Vec2I>> m_spacesCache;
  mutable HashMap<AssetPath, RectU> m_regionCache;
  TrackerListenerPtr m_reloadTracker;
  size_t m_reloadTicks = 0;
};

}

#endif
