vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 3ae885f5b212c6d180b7a66226c812f228b9caf6
  SHA512 dfe5e27020d1ee559c8261ee17ba4fd3bd554b17b0da9b6a419d2b898bb4c4677d2e892fd798607c8afcba02df7f018264a76c2a25f5f3b876b9b8dbed5f4c3a
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
