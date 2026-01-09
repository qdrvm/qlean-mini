vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.5
  SHA512 1c7dbdd0f24a35f1d79e886d6ab271eb8705dbbc9f7e60520e7e57184968976f70fc85f54dba6baa5359e9df83adcc955cb9739f8ebb97c7db830adc4a376f68
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
