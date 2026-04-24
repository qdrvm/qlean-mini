vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF f0e3324be6314ad62bd4cd0706149d82903345b5
  SHA512 c782b850d6a8052ef66def4a7ccb4e480a49bcd53ce4a0ed31aefd934b8bb830b2796cf372c92cafa5a821f3f1cf927cc6fb76e8f1d5205b301e156ac342a0c6
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
