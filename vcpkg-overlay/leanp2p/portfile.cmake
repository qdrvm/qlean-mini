vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF badeae740d8e279bc6450eb0ed85d5e21f413470
  SHA512 aa86b1bc16728289a780467d40b92988b5b9bcd7ff9ab8629e9348b0b48d179778a0bc414c0919dabba97aee252c887a15e69f32661ae4182b762932c42d08bd
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
