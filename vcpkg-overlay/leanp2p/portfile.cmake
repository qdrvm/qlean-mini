vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.1
  SHA512 cd50f92dc8be4839156e02fc5f514d8cb9f6a890a6e552599a2929d3560cb7feeeb2f4529b4a3e461ff90752796f74e82147488f59a8fd5cb9c756305428b946
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
