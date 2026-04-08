vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF 664d207abf352c1070b626a4a8d0f466e01ed913
  SHA512 5c030faf8a5a09e50aee51c6cced74efa95f89c92fd756d75d2661aea9d1df5ebc44a38b648ec82f5551942144afecf678877b0d05d65953265d0829e4c01a40
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
