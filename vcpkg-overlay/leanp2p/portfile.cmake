vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF d9cd796a2ce5efe448ade89565f21df7a73a812e
  SHA512 522c76718e763f5faa3ac1e0348bccb306360aa6c663d626d6903146aae3d7dc24cfd6294e492ab06e9317fad09fee82c46526a485ce627100fd6b8ff291bdf6
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
