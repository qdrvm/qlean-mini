vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF f651d299548d21f753fc7fbd532ec751f671a8f5
  SHA512 a23de40aad275c4edb339ec6cb9b539c30d11a1641459bedb950018119ff283da3d8019c16f01d02db90aa6d9c6a840e4130030cdca1a70942d15608c51867b3
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
