vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 7ab988c0328478483204cf9140a4d894b38fe892 #refs/tags/v0.0.7
  SHA512 1eb5f266e495ba3188ea7fa346e5562ee9fec8b2b7f4a75a38b37e64890494e7f95b04d0a15e4662607267ed0815a0710ddcfe1f72d08af6d26deba3e702530d
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
