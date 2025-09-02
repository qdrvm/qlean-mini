vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/soralog
  REF cf70b08f8ef86696861272bc7195242b718d23cd
  SHA512 43f1bef886ea15de96a98a3f8aa20223e0aa8fb0f2c745b2cc2fbf368a847728602d5422a4b886c11c74a853457b35208ecef2b8f9816a1ff12b1277f854cc26
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
