vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO OffchainLabs/sszpp
  REF 4342a06fe3595384dedc8dc6c1307a14883577bc
  SHA512 a5abea3ad6a1d706428886acfea2a8990623925d5488b23b86c027179282aed0b98928317e946cfb2cbc27e3e230550e6a728cd9e888c6e54b69752b503bf6c9
  PATCHES
    vcpkg.patch
    compute_hashtree_size_inline.patch
)
set(VCPKG_BUILD_TYPE release) # header-only port
vcpkg_cmake_configure(
  SOURCE_PATH ${SOURCE_PATH}
)
vcpkg_cmake_install()
