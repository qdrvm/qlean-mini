#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

# Executables (should contain `main()` function)
add_subdirectory(executable)

# Application's thinks
add_subdirectory(app)

# Blockchain
add_subdirectory(blockchain)

# Cryptography thinks
add_subdirectory(crypto)

# Dependency injection
add_subdirectory(injector)

# Logging subsystem
add_subdirectory(log)

# Metrics subsystem
add_subdirectory(metrics)

# Clocks and time subsystem
add_subdirectory(clock)

# Subscription Engine subsystem
add_subdirectory(se)

# Modules subsystem
add_subdirectory(modules)

# Storage
add_subdirectory(storage)

# Utilities
add_subdirectory(utils)
