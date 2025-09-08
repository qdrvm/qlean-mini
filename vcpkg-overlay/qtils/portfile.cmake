vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF 4eb3f8024817d66932cec0c52e74e127c137a78a
  SHA512 c02b90803a1cbf09dcb0e4707c84b3afdc83449d12ad1771e2918a3cdb40b8d01bda4f93fcb50491e35593fd060ec53c8a4b0b425dbb3df936a32312e5b99859
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
