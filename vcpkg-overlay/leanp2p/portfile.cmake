vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.7
  SHA512 ab84dbcaf6babbe189bdb9cff7fa7763a3a07e6d47ba79e3f7a12c18583d322ce8028553903b3b791b127f67667846a27c884165b8dfc9cfbfa2e0c933cb6f54
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
