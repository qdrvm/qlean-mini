vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.4
  SHA512 9ab9ab55b08306198d2bdaf3b80086126b06b2ac04e6f795e22cb1ba7a44f4ae8d8505183622ba91abc1d0c4e65f3148939e5ccdbef7829b5b625a8bf87139fd
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
