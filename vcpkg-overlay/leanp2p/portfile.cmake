vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 6b6f2defd1ee770aa89b35e3f8155d08aa62f903
  SHA512 7d5c70c40425ced88d1efe25d5f6e485f914b0cd2c32a7e69646b8b250d435c59f04f36f7349f4fae2e869cd117cea421ab0c0ca7e587297eb98b37b194ebc1d
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
