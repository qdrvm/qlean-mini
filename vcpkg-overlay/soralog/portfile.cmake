vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/soralog
  REF 832b2c5421e767bc4027d4d608c9b7281b502c42
  SHA512 9d66b48cf6e014dd952e2b002802e446ac0c6cfdc5706d2ca53fb1a8966ee87c631ca22afe0bb52dd180af2ecc53fe01b878398c28815c744ee7bc96042cae35
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
