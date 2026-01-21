vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF refs/tags/v1.0.7
  SHA512 fe26cfb76ef48bdb8a71637afa2b6e176423b20f157abe226a3bd368eabcfda37ad054de7efb1fea93d8bf3d74b90db7fd295dbe0224a47f6384c43dc6dd33f3
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
