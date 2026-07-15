vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qdrvm-crates
  REF bc2fdaa333a25bb85550a40d4fcc37a5fea9dc5e
  SHA512 0eb8411ff7d3c112402a49eac4ad7737457e311474cb03422902d5f552cfab0031dab7089ff9847fcb07e98075141cf2b9e59915cfc85c18018f5872dcf4db04
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
  "-DQDRVM_BIND_CRATES=c_hash_sig"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qdrvm-crates" CONFIG_PATH "lib/cmake/qdrvm-crates")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
