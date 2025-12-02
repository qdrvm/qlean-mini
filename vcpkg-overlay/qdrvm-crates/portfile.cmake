vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF 83a22b5489d90cf24b276d29970dd425b9d5606e
  SHA512 3a4194c9eade43c489b19cd6eda96e3d70fc0cb1e324ec32d2252675839a3c286d0a8f7694bb6782cb10c86951f8ccdaf3c075544cca3f9f3dcc484199d7826c
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
