vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO OffchainLabs/hashtree
  REF d302036ae757442ee6ef658f7936af819f760892
  SHA512 247b8a6d451fbc334e15979ef4d2f47248b377725f7ee1a7f6f56e546cf236f55f0b240eaff16eceed2165b48c03f3c085063b363b2b8a9a4ea9112cf9bbfd85
  PATCHES
    vcpkg.patch
)
vcpkg_cmake_configure(
  SOURCE_PATH ${SOURCE_PATH}
)
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
