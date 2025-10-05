vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF df148755d526ccff0cb657cad9986659f4681214
  SHA512 4091bd92505f4abaac71c3adc6166a380f1a7a49568fcb45f2dc04ab0447b667174fe6c8da8ea360a3b1b52008c188ddb5e6ca710e7d59813698c48899603365
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
