vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 29f1937bf74f2d9dfddcc3774d670240d131b9d4
  SHA512 66a4a9e329aadb031dc7274aa73c358b4fb2372af7d0a86043584571a65e7951454f78c7a3955cbe05dda9fbac3587a1108b3bdf63390e130d5ea7e806e59499
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
