vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF b66a56fa691e2aaa879f61527bab37d266bc3f5e
  SHA512 fc8163e8b2aa0263d39aeb30617daff387badb3858ea69571c27417a66491a64e257d1558be71ecb10ca9a3d53064606883d10e77901455ef4e094cecec565d7
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
