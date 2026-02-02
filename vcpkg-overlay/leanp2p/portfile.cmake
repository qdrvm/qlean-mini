vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 1a46fef5262952ef5e0f29451a6050f684c3cdae
  SHA512 add73ce9b0428c911c9ec7134cd20885335d2611ea90e86e91eacb653671739f1c263a16a69225ad565ec6541e437bdd0cf1754b9b27e48a9edcd73f42e08dad
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
