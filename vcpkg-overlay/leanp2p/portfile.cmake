vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF refs/tags/v0.0.8
  SHA512 541ee0dcaa26a464e74db3a86dec930e2d25d04f329bd7bebf1788f29a455ba0f1642076b3c5e4bc83a80442039e67c6add8270c88f69a1748b2737adcff66a6
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
