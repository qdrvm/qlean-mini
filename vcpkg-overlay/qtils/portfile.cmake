vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF d139eba58107bdc726a547f144519d8ad4ae2347
  SHA512 8cb1946283f1b568cd3788039ee76fafc4a7d3c3db7da294103c534f7a0f6309e55e7f781cbb8acbcc3ae24162caaaf8800b4ac57b2406b4f492c81e44905b1e
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
