vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.2
  SHA512 72a82d313ca84fe0095c755daa733cfdd09609080a6a9f627587355d57d0bf4a34448884282f29ebd1e23fcc21d8f5181ada63ff3d87772e060e5e9f554406a2
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
