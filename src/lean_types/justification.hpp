/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <lean_types/types.hpp>
#include <qtils/byte_vec.hpp>
#include <qtils/tagged.hpp>

namespace lean {

  using Justification = qtils::Tagged<qtils::ByteVec, struct JustificationTag>;

}
