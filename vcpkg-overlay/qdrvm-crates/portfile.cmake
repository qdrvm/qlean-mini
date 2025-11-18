vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF a3aa7c7ef16fd61fcf3f9e78fe32aa9358ef3df9
  SHA512 875dc10557acaacf94232d053f6387a5cfc0ce5bf56296844fa04b26af922337d527efc7b145bf1956fa349a664c2d37638f53d60f0271088a0ffd84a1bb90c2
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
