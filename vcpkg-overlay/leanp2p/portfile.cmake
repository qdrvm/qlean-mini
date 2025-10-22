vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 56c1859237f454e6e029a54b0d71f3d6fb420404
  SHA512 19fce0c1463e85aff685634f2c3d8da17ff8899c2490e7d9f579818006fedd505b26a4b6b42eb5378fe0c494aceb4a7826a52fee6c29c9f97a6bcedb5a4307a5
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
