#include "Zen4Topdown.h"

#include "../Services.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "services/topdown/TopdownCalculator.h"

#include <algorithm>

namespace cali
{
namespace topdown
{

Zen4Topdown::Zen4Topdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
        level,
        // top_counters
        "DISPATCH_STALLS_1:FE_NO_OPS,"
        "CYCLES_NOT_IN_HALT,"
        "OPS_SOURCE_DISPATCHED_FROM_DECODER:DECODER,"
        "OPS_SOURCE_DISPATCHED_FROM_DECODER:OPCACHE,"
        "OPS_SOURCE_DISPATCHED_FROM_DECODER:LOOP_BUFFER,"
        "RETIRED_OPS,"
        "DISPATCH_STALLS_1:BE_STALLS,"
        "DISPATCH_STALLS_1:SMT_CONTENTION",
        // all_counters
        "DISPATCH_STALLS_1:FE_NO_OPS,"
        "CYCLES_NOT_IN_HALT,"
        "OPS_SOURCE_DISPATCHED_FROM_DECODER:DECODER,"
        "OPS_SOURCE_DISPATCHED_FROM_DECODER:OPCACHE,"
        "OPS_SOURCE_DISPATCHED_FROM_DECODER:LOOP_BUFFER,"
        "RETIRED_OPS,"
        "DISPATCH_STALLS_1:BE_STALLS,"
        "DISPATCH_STALLS_1:SMT_CONTENTION,"
        "RETIRED_BRANCH_INSTRUCTIONS_MISPREDICTED,"
        "RESYNCS,"
        "CYCLES_NO_RETIRE:NOT_COMPLETE_MISING_LOAD,"
        "CYCLES_NO_RETIRE:NOT_COMPLETE_LOAD_AND_ALU,"
        "RETIRED_UCODE_OPS,"
        "DISPATCH_STALLS_1:FE_NO_OPS:c=6",
        // res_top
        { "frontend_bound", "bad_speculation", "backend_bound", "retiring", "smt_contention" },
        // res_all
        { "frontend_bound",
          "fetch_latency",
          "fetch_bandwidth",
          "bad_speculation",
          "branch_mispredicts",
          "pipeline_restarts",
          "backend_bound",
          "memory_bound",
          "core_bound",
          "retiring",
          "fastpath",
          "microcode",
          "smt_contention" }
    )
{}

bool Zen4Topdown::setup_config(Caliper& c, Channel& channel) const
{
    channel.config().set("CALI_PAPI_COUNTERS", m_level == All ? m_all_counters : m_top_counters);
    channel.config().set("CALI_PAPI_ENABLE_MULTIPLEXING", "true");
    if (!cali::services::register_service(&c, &channel, "papi")) {
        Log(0).stream() << channel.name() << ": topdown: Unable to register papi service, skipping topdown"
                        << std::endl;
        return false;
    }
    return true;
}

std::vector<Entry> Zen4Topdown::compute_toplevel(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_no_dispatch_per_slot_no_ops_from_fe = get_val_from_rec(rec, "DISPATCH_STALLS_1:FE_NO_OPS");
    Variant v_src_op_disp_decoder                 = get_val_from_rec(rec, "OPS_SOURCE_DISPATCHED_FROM_DECODER:DECODER");
    Variant v_src_op_disp_opcache                 = get_val_from_rec(rec, "OPS_SOURCE_DISPATCHED_FROM_DECODER:OPCACHE");
    Variant v_src_op_disp_loop_buffer = get_val_from_rec(rec, "OPS_SOURCE_DISPATCHED_FROM_DECODER:LOOP_BUFFER");
    Variant v_ex_ret_ops              = get_val_from_rec(rec, "RETIRED_OPS");
    Variant v_no_dispatch_per_slot_backend_stalls = get_val_from_rec(rec, "DISPATCH_STALLS_1:BE_STALLS");
    Variant v_no_dispatch_per_slot_smt_contention = get_val_from_rec(rec, "DISPATCH_STALLS_1:SMT_CONTENTION");
    Variant v_ls_not_halted_cyc                   = get_val_from_rec(rec, "CYCLES_NOT_IN_HALT");

    bool is_incomplete = v_no_dispatch_per_slot_no_ops_from_fe.empty() || v_src_op_disp_decoder.empty()
                         || v_src_op_disp_opcache.empty() || v_src_op_disp_loop_buffer.empty() || v_ex_ret_ops.empty()
                         || v_no_dispatch_per_slot_backend_stalls.empty()
                         || v_no_dispatch_per_slot_smt_contention.empty() || v_ls_not_halted_cyc.empty();
    bool is_nonzero = v_no_dispatch_per_slot_no_ops_from_fe.to_double() > 0.0 && v_src_op_disp_decoder.to_double() > 0.0
                      && v_src_op_disp_opcache.to_double() > 0.0 && v_src_op_disp_loop_buffer.to_double() > 0.0
                      && v_ex_ret_ops.to_double() > 0.0 && v_no_dispatch_per_slot_backend_stalls.to_double() > 0.0
                      && v_no_dispatch_per_slot_smt_contention.to_double() > 0.0
                      && v_ls_not_halted_cyc.to_double() > 0.0;

    if (is_incomplete || !is_nonzero)
        return ret;

    double total_dispatch_slots = 6.0 * v_ls_not_halted_cyc.to_double();
    double src_op_disp_all =
        v_src_op_disp_decoder.to_double() + v_src_op_disp_opcache.to_double() + v_src_op_disp_loop_buffer.to_double();

    if (src_op_disp_all < v_ex_ret_ops.to_double()) {
        Log(1).stream() << "topdown: value for de_src_op_disp.all is less than ex_ret_ops, which is invalid";
        return ret;
    }

    double frontend_bound  = std::max(v_no_dispatch_per_slot_no_ops_from_fe.to_double() / total_dispatch_slots, 0.0);
    double bad_speculation = std::max((src_op_disp_all - v_ex_ret_ops.to_double()) / total_dispatch_slots, 0.0);
    double backend_bound   = std::max(v_no_dispatch_per_slot_backend_stalls.to_double() / total_dispatch_slots, 0.0);
    double retiring        = std::max(v_ex_ret_ops.to_double() / total_dispatch_slots, 0.0);
    double smt_contention  = std::max(v_no_dispatch_per_slot_smt_contention.to_double() / total_dispatch_slots, 0.0);

    ret.reserve(5);

    ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(frontend_bound)));
    ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(bad_speculation)));
    ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(backend_bound)));
    ret.push_back(Entry(m_result_attrs["retiring"], Variant(retiring)));
    ret.push_back(Entry(m_result_attrs["smt_contention"], Variant(smt_contention)));

    return ret;
}

std::size_t Zen4Topdown::get_num_expected_toplevel() const
{
    return 5;
}

std::vector<Entry> Zen4Topdown::compute_retiring(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_ex_ret_ops        = get_val_from_rec(rec, "RETIRED_OPS");
    Variant v_ex_ret_ucode_ops  = get_val_from_rec(rec, "RETIRED_UCODE_OPS");
    Variant v_ls_not_halted_cyc = get_val_from_rec(rec, "CYCLES_NOT_IN_HALT");

    bool is_incomplete = v_ex_ret_ops.empty() || v_ex_ret_ucode_ops.empty() || v_ls_not_halted_cyc.empty();

    if (is_incomplete)
        return ret;

    double total_dispatch_slots = 6.0 * v_ls_not_halted_cyc.to_double();
    double retiring             = std::max(v_ex_ret_ops.to_double() / total_dispatch_slots, 0.0);

    double fastpath  = std::max(retiring * (1 - (v_ex_ret_ucode_ops.to_double() / v_ex_ret_ops.to_double())), 0.0);
    double microcode = std::max(retiring * (v_ex_ret_ucode_ops.to_double() / v_ex_ret_ops.to_double()), 0.0);

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["fastpath"], Variant(fastpath)));
    ret.push_back(Entry(m_result_attrs["microcode"], Variant(microcode)));

    return ret;
}

std::size_t Zen4Topdown::get_num_expected_retiring() const
{
    return 2;
}

std::vector<Entry> Zen4Topdown::compute_backend_bound(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_no_dispatch_per_slot_backend_stalls = get_val_from_rec(rec, "DISPATCH_STALLS_1:BE_STALLS");
    Variant v_ls_not_halted_cyc                   = get_val_from_rec(rec, "CYCLES_NOT_IN_HALT");
    Variant v_ex_no_retire_load_not_complete      = get_val_from_rec(rec, "CYCLES_NO_RETIRE:NOT_COMPLETE_MISSING_LOAD");
    Variant v_ex_no_retire_not_complete           = get_val_from_rec(rec, "CYCLES_NO_RETIRE:NOT_COMPLETE_LOAD_AND_ALU");

    bool is_incomplete = v_no_dispatch_per_slot_backend_stalls.empty() || v_ls_not_halted_cyc.empty()
                         || v_ex_no_retire_load_not_complete.empty() || v_ex_no_retire_not_complete.empty();

    if (is_incomplete)
        return ret;

    double total_dispatch_slots = 6.0 * v_ls_not_halted_cyc.to_double();
    double backend_bound = std::max(v_no_dispatch_per_slot_backend_stalls.to_double() / total_dispatch_slots, 0.0);

    double memory_bound = std::max(
        backend_bound * (v_ex_no_retire_load_not_complete.to_double() / v_ex_no_retire_not_complete.to_double()),
        0.0
    );
    double core_bound = std::max(
        backend_bound * (1 - (v_ex_no_retire_load_not_complete.to_double() / v_ex_no_retire_not_complete.to_double())),
        0.0
    );

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["memory_bound"], Variant(memory_bound)));
    ret.push_back(Entry(m_result_attrs["core_bound"], Variant(core_bound)));

    return ret;
}

std::size_t Zen4Topdown::get_num_expected_backend_bound() const
{
    return 2;
}

std::vector<Entry> Zen4Topdown::compute_frontend_bound(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_no_dispatch_per_slot_no_ops_from_fe_cmask_6 = get_val_from_rec(rec, "DISPATCH_STALLS_1:FE_NO_OPS:c=6");
    Variant v_no_dispatch_per_slot_no_ops_from_fe         = get_val_from_rec(rec, "DISPATCH_STALLS_1:FE_NO_OPS");
    Variant v_ls_not_halted_cyc                           = get_val_from_rec(rec, "CYCLES_NOT_IN_HALT");

    bool is_incomplete = v_no_dispatch_per_slot_no_ops_from_fe_cmask_6.empty()
                         || v_no_dispatch_per_slot_no_ops_from_fe.empty() || v_ls_not_halted_cyc.empty();

    if (is_incomplete)
        return ret;

    double total_dispatch_slots = 6.0 * v_ls_not_halted_cyc.to_double();

    double fetch_latency =
        std::max((6 * v_no_dispatch_per_slot_no_ops_from_fe_cmask_6.to_double()) / total_dispatch_slots, 0.0);
    double fetch_bandwidth = std::max(
        (v_no_dispatch_per_slot_no_ops_from_fe.to_double()
         - (6 * v_no_dispatch_per_slot_no_ops_from_fe_cmask_6.to_double()))
            / total_dispatch_slots,
        0.0
    );

    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["fetch_latency"], Variant(fetch_latency)));
    ret.push_back(Entry(m_result_attrs["fetch_bandwidth"], Variant(fetch_bandwidth)));

    return ret;
}

std::size_t Zen4Topdown::get_num_expected_frontend_bound() const
{
    return 2;
}

std::vector<Entry> Zen4Topdown::compute_bad_speculation(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_src_op_disp_decoder     = get_val_from_rec(rec, "OPS_SOURCE_DISPATCHED_FROM_DECODER:DECODER");
    Variant v_src_op_disp_opcache     = get_val_from_rec(rec, "OPS_SOURCE_DISPATCHED_FROM_DECODER:OPCACHE");
    Variant v_src_op_disp_loop_buffer = get_val_from_rec(rec, "OPS_SOURCE_DISPATCHED_FROM_DECODER:LOOP_BUFFER");
    Variant v_ex_ret_ops              = get_val_from_rec(rec, "RETIRED_OPS");
    Variant v_ex_ret_brn_misp         = get_val_from_rec(rec, "RETIRED_BRANCH_INSTRUCTIONS_MISPREDICTED");
    Variant v_resyncs_or_nc_redirects = get_val_from_rec(rec, "RESYNCS");
    Variant v_ls_not_halted_cyc       = get_val_from_rec(rec, "CYCLES_NOT_IN_HALT");

    bool is_incomplete = v_src_op_disp_decoder.empty() || v_src_op_disp_opcache.empty()
                         || v_src_op_disp_loop_buffer.empty() || v_ex_ret_ops.empty() || v_ex_ret_brn_misp.empty()
                         || v_resyncs_or_nc_redirects.empty() || v_ls_not_halted_cyc.empty();

    if (is_incomplete)
        return ret;

    double total_dispatch_slots = 6.0 * v_ls_not_halted_cyc.to_double();
    double src_op_disp_all =
        v_src_op_disp_decoder.to_double() + v_src_op_disp_opcache.to_double() + v_src_op_disp_loop_buffer.to_double();
    double bad_speculation = std::max((src_op_disp_all - v_ex_ret_ops.to_double()) / total_dispatch_slots, 0.0);

    double branch_mispredicts = std::max(
        ((bad_speculation * v_ex_ret_brn_misp.to_double())
         / (v_ex_ret_brn_misp.to_double() + v_resyncs_or_nc_redirects.to_double())),
        0.0
    );
    double pipeline_restarts = std::max(
        (bad_speculation * v_resyncs_or_nc_redirects.to_double())
            / (v_ex_ret_brn_misp.to_double() + v_resyncs_or_nc_redirects.to_double()),
        0.0
    );

    ret.reserve(2);

    ret.push_back(Entry(m_result_attrs["branch_mispredicts"], Variant(branch_mispredicts)));
    ret.push_back(Entry(m_result_attrs["pipeline_restarts"], Variant(pipeline_restarts)));

    return ret;
}

std::size_t Zen4Topdown::get_num_expected_bad_speculation() const
{
    return 2;
}

} // namespace topdown
} // namespace cali