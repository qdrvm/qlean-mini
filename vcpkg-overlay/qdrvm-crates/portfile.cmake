vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF refs/tags/v1.0.6
  SHA512 bede49cb4d42ada9daefee57b8b2ba7726231fcfbdfff28ed8c0f91fdb97cb56bfdc4baef611943454424668bdc9bc2fd9e284426934e19bb822452c10cf7be9
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
