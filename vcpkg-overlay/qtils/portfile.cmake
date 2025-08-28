vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF 77479adc6fa4eeceb81dbf78ecab7766fdb7c898
  SHA512 6461d39c7175138ff7c5790c5158384ab29241a5946c5604b3a5b39c6fa7b593d3fda1da9cd556f4e12e4d553dfbd9a8b5a3bda183ee9d9972570b9883cfa510
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
