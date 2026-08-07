
#include "srsran/e2/e2sm/e2sm_runOran/ta_indication_store.h"

#include <mutex>

namespace srsran {
namespace {

std::mutex                   ta_mutex;
std::vector<ta_indication_record> ta_records;

} // namespace

void push_ta_indication(ta_indication_record record)
{
  std::lock_guard<std::mutex> lock(ta_mutex);
  ta_records.push_back(record);
}

std::vector<ta_indication_record> drain_ta_indications()
{
  std::lock_guard<std::mutex> lock(ta_mutex);
  std::vector<ta_indication_record> drained;
  drained.swap(ta_records);
  return drained;
}

} // namespace srsran
