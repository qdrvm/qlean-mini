/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_vec.hpp>
#include <qtils/tagged.hpp>
#include <types/types.hpp>

namespace lean {

  using Justification = qtils::Tagged<qtils::ByteVec, struct JustificationTag>;

}
