vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF refs/tags/v0.1.4
  SHA512 124f3711eb64df3a2e207bff8bf953ccc2dfa838f21da72a1cc77c8aec95def350e70607adf9d8e7123e56d5bffcf830052f607dfa12badc7efe463bd0be747c
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
