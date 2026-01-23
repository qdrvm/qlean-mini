vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.6
  SHA512 13dd0a8098e5d63a54600ed7303cd4df78a6bcd858a08cbf33a5274f94edba9166e59573a34e7f13eb823551a036a2c6e650ba66be66643355ca94e8d111031d
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
