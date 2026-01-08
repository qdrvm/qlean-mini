vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 90ab756da131995932671400419ac8e562a4b5e6
  SHA512 790ee0a4c0301810b9e3d6949cfaedc579bd5f0e52d4d2ec1f97bc16767886b26864979ee0a37377b73835bd252510f9f19f3954fb325dac55f8c5c42dafabaf
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
