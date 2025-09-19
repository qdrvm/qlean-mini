vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF refs/tags/v0.1.5
  SHA512 12fe763fdfab70bb90fb8687efdde63bb0b04c0e3f50efea73115997ed278892e99e2b49fa13b9fa80d6e4740dc0c9942c16162c31ab3890e4eb09e1f7a81bc4
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
