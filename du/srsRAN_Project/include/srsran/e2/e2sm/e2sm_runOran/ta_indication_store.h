/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "srsran/ran/du_types.h"
#include <cstdint>
#include <vector>

namespace srsran {

struct ta_indication_record {
  du_ue_index_t ue_index      = INVALID_DU_UE_INDEX;
  uint32_t      rnti          = 0;
  uint64_t      timestamp_us  = 0;
  uint32_t      timing_advance = 0;
  uint32_t      source         = 0;
  bool          attached       = true;
};

void push_ta_indication(ta_indication_record record);
std::vector<ta_indication_record> drain_ta_indications();

} // namespace srsran
