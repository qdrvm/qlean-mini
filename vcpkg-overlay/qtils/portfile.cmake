vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF refs/tags/v0.1.3
  SHA512 09e759f82ce273b602ec851ed7b5bb5fe1e0471a35a9874d04190946a38b8cf5dcea0923af91db798c53c01970c7bfe3452fb956efd66e067ecbec3e0c99fb02
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
