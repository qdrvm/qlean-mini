vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 3df3b87b4af0a2427613a9390354c2f2f937894e
  SHA512 cdbc5636a09d1eaad498a6f596ec90826890e9acc5de4a65ede8e0d55d8a107f8d4f533d82e8c293ab8dc5b1108b742a9d916e1cf89347965da740ee9b7aa166
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
