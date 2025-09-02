vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/kagome-crates
  REF ab20d915257159a3499f1355168831ce01e1c063
  SHA512 8aee6dedf09c2f8aa660c2e400672ed9d652529ccb6dddde0110d7e929712462853903981e5b0549ece20b0a37e246cc5a14a0a6502169f7fe497ebff0e03ebe
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=schnorrkel;ark_vrf"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
