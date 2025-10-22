vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.3
  SHA512 da5ef7a621f2758e588972b958fc35706ddfd9a0ee9fd378c113cbe0348a28788acc09fa7e23b6c663f9f5b4d1448b245e7610f07ff02b80be4a074a9e27a102
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
