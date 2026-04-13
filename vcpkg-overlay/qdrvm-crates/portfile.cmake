vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF 58dd670b3acc7d36a97a52983712ee3e18a0c9ab
  SHA512 185c1c89555a9a299ff8a456fa8c96b9c797a80940e47b891f35ce34283adec9ac4f4b858cfdbeda89d536de7e29c1d1743a297745984e59c1bdbdd614d7864a
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
