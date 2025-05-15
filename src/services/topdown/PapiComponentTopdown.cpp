#include "PapiComponentTopdown.h"

#include "../Services.h"

#include "caliper/common/Entry.h"
#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"
#include "services/topdown/TopdownCalculator.h"
#include <algorithm>

namespace cali
{
namespace topdown
{

PapiComponentTopdown::PapiComponentTopdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
        level,
        // top_counters
        "TOPDOWN_RETIRING_PERC"
        ",TOPDOWN_BAD_SPEC_PERC"
        ",TOPDOWN_FE_BOUND_PERC"
        ",TOPDOWN_BE_BOUND_PERC",
        // all_counters
        "TOPDOWN_RETIRING_PERC"
        ",TOPDOWN_BAD_SPEC_PERC"
        ",TOPDOWN_FE_BOUND_PERC"
        ",TOPDOWN_BE_BOUND_PERC"
        ",TOPDOWN_HEAVY_OPS_PERC"
        ",TOPDOWN_LIGHT_OPS_PERC"
        ",TOPDOWN_BR_MISPREDICT_PERC"
        ",TOPDOWN_MACHINE_CLEARS_PERC"
        ",TOPDOWN_FETCH_LAT_PERC"
        ",TOPDOWN_FETCH_BAND_PERC"
        ",TOPDOWN_MEM_BOUND_PERC"
        ",TOPDOWN_CORE_BOUND_PERC",
        // res_top
        { "retiring", "backend_bound", "frontend_bound", "bad_speculation" },
        // res_all
        { "retiring",
          "backend_bound",
          "frontend_bound",
          "bad_speculation",
          "branch_mispredict",
          "machine_clears",
          "frontend_latency",
          "frontend_bandwidth",
          "memory_bound",
          "core_bound",
          "light_ops",
          "heavy_ops" }
    )
{}

bool PapiComponentTopdown::setup_config(Caliper& c, Channel& channel) const
{
    channel.config().set("CALI_PAPI_COUNTERS", m_level == All ? m_all_counters : m_top_counters);

    if (!cali::services::register_service(&c, &channel, "papi")) {
        Log(0).stream() << channel.name() << ": topdown: Unable to register papi service, skipping topdown"
                        << std::endl;
        return false;
    }

    return true;
}

std::vector<Entry> PapiComponentTopdown::compute_toplevel(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    // Get PAPI metrics for toplevel
    Variant retiring = get_val_from_rec(rec, "TOPDOWN_RETIRING_PERC");
    Variant bad_spec = get_val_from_rec(rec, "TOPDOWN_BAD_SPEC_PERC");
    Variant fe_bound = get_val_from_rec(rec, "TOPDOWN_FE_BOUND_PERC");
    Variant be_bound = get_val_from_rec(rec, "TOPDOWN_BE_BOUND_PERC");

    // Check if any Variant is empty
    bool is_incomplete = retiring.empty() || bad_spec.empty() || fe_bound.empty() || be_bound.empty();
    // Check if all Variants are greater than 0 when casted to doubles
    bool is_nonzero = retiring.to_double() > 0.0 && bad_spec.to_double() > 0.0 && fe_bound.to_double() > 0.0
                      && be_bound.to_double() > 0.0;

    if (is_incomplete || !is_nonzero)
        return ret;

    ret.reserve(4);
    ret.push_back(Entry(m_result_attrs["retiring"], Variant(std::max(retiring.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(std::max(be_bound.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(std::max(fe_bound.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(std::max(bad_spec.to_double(), 0.0))));

    return ret;
}

std::size_t PapiComponentTopdown::get_num_expected_toplevel() const
{
    return 4;
}

std::vector<Entry> PapiComponentTopdown::compute_retiring(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    // Get PAPI metrics for toplevel
    Variant heavy_ops = get_val_from_rec(rec, "TOPDOWN_HEAVY_OPS_PERC");
    Variant light_ops = get_val_from_rec(rec, "TOPDOWN_LIGHT_OPS_PERC");

    // Check if any Variant is empty
    bool is_incomplete = heavy_ops.empty() || light_ops.empty();
    // Check if all Variants are greater than 0 when casted to doubles
    bool is_nonzero = heavy_ops.to_double() > 0.0 && light_ops.to_double() > 0.0;

    if (is_incomplete || !is_nonzero)
        return ret;

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["heavy_ops"], Variant(std::max(heavy_ops.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["light_ops"], Variant(std::max(light_ops.to_double(), 0.0))));

    return ret;
}

std::size_t PapiComponentTopdown::get_num_expected_retiring() const
{
    return 2;
}

std::vector<Entry> PapiComponentTopdown::compute_backend_bound(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    // Get PAPI metrics for toplevel
    Variant memory_bound = get_val_from_rec(rec, "TOPDOWN_MEM_BOUND_PERC");
    Variant core_bound   = get_val_from_rec(rec, "TOPDOWN_CORE_BOUND_PERC");

    // Check if any Variant is empty
    bool is_incomplete = memory_bound.empty() || core_bound.empty();
    // Check if all Variants are greater than 0 when casted to doubles
    bool is_nonzero = memory_bound.to_double() > 0.0 && core_bound.to_double() > 0.0;

    if (is_incomplete || !is_nonzero)
        return ret;

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["memory_bound"], Variant(std::max(memory_bound.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["core_bound"], Variant(std::max(core_bound.to_double(), 0.0))));

    return ret;
}

std::size_t PapiComponentTopdown::get_num_expected_backend_bound() const
{
    return 2;
}

std::vector<Entry> PapiComponentTopdown::compute_frontend_bound(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    // Get PAPI metrics for toplevel
    Variant fe_lat       = get_val_from_rec(rec, "TOPDOWN_FETCH_LAT_PERC");
    Variant fe_bandwidth = get_val_from_rec(rec, "TOPDOWN_FETCH_BAND_PERC");

    // Check if any Variant is empty
    bool is_incomplete = fe_lat.empty() || fe_bandwidth.empty();
    // Check if all Variants are greater than 0 when casted to doubles
    bool is_nonzero = fe_lat.to_double() > 0.0 && fe_bandwidth.to_double() > 0.0;

    if (is_incomplete || !is_nonzero)
        return ret;

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["frontend_latency"], Variant(std::max(fe_lat.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["frontend_bandwidth"], Variant(std::max(fe_bandwidth.to_double(), 0.0))));

    return ret;
}

std::size_t PapiComponentTopdown::get_num_expected_frontend_bound() const
{
    return 2;
}

std::vector<Entry> PapiComponentTopdown::compute_bad_speculation(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    // Get PAPI metrics for toplevel
    Variant br_mispred     = get_val_from_rec(rec, "TOPDOWN_BR_MISPREDICT_PERC");
    Variant machine_clears = get_val_from_rec(rec, "TOPDOWN_MACHINE_CLEARS_PERC");

    // Check if any Variant is empty
    bool is_incomplete = br_mispred.empty() || machine_clears.empty();
    // Check if all Variants are greater than 0 when casted to doubles
    bool is_nonzero = br_mispred.to_double() > 0.0 && machine_clears.to_double() > 0.0;

    if (is_incomplete || !is_nonzero)
        return ret;

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["branch_mispredict"], Variant(std::max(br_mispred.to_double(), 0.0))));
    ret.push_back(Entry(m_result_attrs["machine_clears"], Variant(std::max(machine_clears.to_double(), 0.0))));

    return ret;
}

std::size_t PapiComponentTopdown::get_num_expected_bad_speculation() const
{
    return 2;
}

} // namespace topdown
} // namespace cali