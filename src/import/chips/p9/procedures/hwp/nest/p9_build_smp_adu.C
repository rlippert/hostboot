/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/nest/p9_build_smp_adu.C $  */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2017                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
///
/// @file p9_build_smp_adu.C
/// @brief Interface for ADU operations required to support fabric
///        configuration actions (FAPI2)
///
/// *HWP HWP Owner: Joe McGill <jmcgill@us.ibm.com>
/// *HWP FW Owner: Thi Tran <thi@us.ibm.com>
/// *HWP Team: Nest
/// *HWP Level: 3
/// *HWP Consumed by: HB,FSP
///

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include <p9_build_smp_adu.H>
#include <p9_adu_coherent_utils.H>
#include <p9_perv_scom_addresses.H>
#include <p9_misc_scom_addresses.H>
#include <p9_misc_scom_addresses_fld.H>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

// poll threshold
const uint32_t P9_BUILD_SMP_MAX_STATUS_POLLS = 100;

// ADU lock
const uint32_t P9_BUILD_SMP_PHASE1_ADU_LOCK_ATTEMPTS = 1;
const bool P9_BUILD_SMP_PHASE1_ADU_PICK_LOCK = false;
const uint32_t P9_BUILD_SMP_PHASE2_ADU_LOCK_ATTEMPTS = 5;
const bool P9_BUILD_SMP_PHASE2_ADU_PICK_LOCK = true;

// FFDC logging on ADU switch fails
const uint8_t P9_BUILD_SMP_FFDC_NUM_REGS = 15;
const uint32_t P9_BUILD_SMP_FFDC_REGS[P9_BUILD_SMP_FFDC_NUM_REGS] =
{
    PU_PB_CENT_SM0_PB_CENT_MODE,          // 0  (FBC Mode)
    PU_PB_CENT_SM0_PB_CENT_HP_MODE_CURR,  // 1  (FBC HP Mode)
    PU_PB_CENT_SM0_PB_CENT_HP_MODE_NEXT,  // 2
    PU_PB_CENT_SM0_PB_CENT_HPX_MODE_CURR, // 3  (FBC HPx Mode)
    PU_PB_CENT_SM0_PB_CENT_HPX_MODE_NEXT, // 4
    PU_PB_CENT_SM0_PB_CENT_HPA_MODE_CURR, // 5  (FBC HPa Mode)
    PU_PB_CENT_SM0_PB_CENT_HPA_MODE_NEXT, // 6
    PERV_XB_CPLT_CONF1,                   // 7  (Electrical IOvalids)
    PU_PB_IOE_FIR_REG,                    // 8  (Electrical TL train states)
    PERV_OB0_CPLT_CONF1,                  // 9  (Optical IOValids)
    PERV_OB1_CPLT_CONF1,                  // 10
    PERV_OB2_CPLT_CONF1,                  // 11
    PERV_OB3_CPLT_CONF1,                  // 12
    PU_IOE_PB_IOO_FIR_REG,                // 13 (Optical TL train states)
    PU_SND_MODE_REG                       // 14
};


//------------------------------------------------------------------------------
// Function definitions
//------------------------------------------------------------------------------


///
/// @brief Check ADU status matches expected state/value
///        NOTE: intended to be run while holding ADU lock
///
/// @param[in] i_target                P9 target
/// @param[in] i_smp                   Structure encapsulating SMP topology
/// @return fapi2:ReturnCode. FAPI2_RC_SUCCESS if success, else error code.
///
fapi2::ReturnCode p9_build_smp_adu_check_status(
    const fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP>& i_target,
    p9_build_smp_system& i_smp)
{
    FAPI_DBG("Start");
    fapi2::ReturnCode l_rc;
    uint32_t l_num_polls = 0;
    bool l_busy_bit_status = false;

    // SMP SWITCH expect BUSY BIT to be clear
    adu_status_busy_handler l_busy_handling = EXIT_ON_BUSY;

    // Wait for operation to be completed (busy bit cleared)
    while (l_num_polls < P9_BUILD_SMP_MAX_STATUS_POLLS)
    {
        l_rc = p9_adu_coherent_status_check(i_target,
                                            l_busy_handling,
                                            true,
                                            l_busy_bit_status);

        if (l_rc)
        {
            FAPI_ERR("p9_adu_coherent_status_check() returns error");
            break;
        }

        if (l_busy_bit_status == true)
        {
            l_num_polls++;

            // last try, set handler to expect busy bit clear, if not then
            // p9_adu_coherent_status_check() will log an error so that
            // we don't have to deal with the error separately here.
            if (l_num_polls == (P9_BUILD_SMP_MAX_STATUS_POLLS - 1))
            {
                l_busy_handling = EXPECTED_BUSY_BIT_CLEAR;
            }
        }
        else
        {
            // Operation done, break out
            break;
        }
    }

    FAPI_DBG("Num of polls %u", l_num_polls);

    if (l_rc)
    {
        fapi2::current_err = l_rc;
        // collect FFDC
        fapi2::variable_buffer l_chip_data_valid(8 * P9_BUILD_SMP_MAX_SIZE);
        fapi2::variable_buffer l_group_ids(8 * P9_BUILD_SMP_MAX_SIZE);
        fapi2::variable_buffer l_chip_ids(8 * P9_BUILD_SMP_MAX_SIZE);
        fapi2::variable_buffer l_ffdc_addrs(64 * P9_BUILD_SMP_FFDC_NUM_REGS);
        fapi2::variable_buffer l_ffdc_reg_data(64 * P9_BUILD_SMP_MAX_SIZE * P9_BUILD_SMP_FFDC_NUM_REGS);
        uint8_t l_idx = 0;

        // init buffers
        l_chip_data_valid.flush<0>();
        l_group_ids.flush<1>();
        l_chip_ids.flush<1>();
        l_ffdc_reg_data.flush<1>();

        for (uint8_t jj = 0; jj < P9_BUILD_SMP_FFDC_NUM_REGS; jj++)
        {
            l_ffdc_addrs.set<uint64_t>(jj, P9_BUILD_SMP_FFDC_REGS[jj]);
        }

        // extract FFDC data
        for (auto n_iter = i_smp.groups.begin();
             n_iter != i_smp.groups.end();
             n_iter++)
        {
            for (auto p_iter = n_iter->second.chips.begin();
                 p_iter != n_iter->second.chips.end();
                 p_iter++)
            {
                // mark valid
                (void) l_chip_data_valid.set<uint8_t>(l_idx, 0x1);
                // log group/chip ID
                (void) l_group_ids.set<uint8_t>(l_idx, n_iter->first);
                (void) l_chip_ids.set<uint8_t>(l_idx, p_iter->first);

                // collect SCOM data
                for (uint8_t jj = 0; jj < P9_BUILD_SMP_FFDC_NUM_REGS; jj++)
                {
                    fapi2::buffer<uint64_t> l_scom_data;
                    fapi2::ReturnCode l_scom_rc;
                    // discard bad SCOM return codes, mark data as all ones
                    // and keep collecting
                    l_scom_rc = fapi2::getScom(*(p_iter->second.target),
                                               P9_BUILD_SMP_FFDC_REGS[jj],
                                               l_scom_data);

                    if (l_scom_rc)
                    {
                        l_scom_data.flush<1>();
                    }

                    (void) l_ffdc_reg_data.set<uint64_t>((P9_BUILD_SMP_FFDC_NUM_REGS * l_idx) + jj,
                                                         l_scom_data());
                }

                l_idx++;
            }
        }

        FAPI_ASSERT(false,
                    fapi2::P9_BUILD_SMP_ADU_STATUS_MISMATCH_ERR()
                    .set_TARGET(i_target)
                    .set_ADU_NUM_POLLS(l_num_polls)
                    .set_CHIP_DATA_VALID(&l_chip_data_valid)
                    .set_GROUP_IDS(l_group_ids)
                    .set_CHIP_IDS(l_chip_ids)
                    .set_FFDC_ADDRS(l_ffdc_addrs)
                    .set_FFDC_REG_DATA(l_ffdc_reg_data),
                    "Status mismatch detected on ADU operation");
    }

fapi_try_exit:
    FAPI_DBG("End");
    return fapi2::current_err;
}


///
/// @brief Set action which will occur on fabric pmisc switch command
///        NOTE: intended to be run while holding ADU lock
///
/// @param[in] i_target        Processor chip target
/// @param[in] i_switch_ab     Perform switch AB operation?
/// @param[in] i_switch_cd     Perform switch CD operation?
///
/// @return fapi2:ReturnCode. FAPI2_RC_SUCCESS if success, else error code.
///
fapi2::ReturnCode p9_build_smp_adu_set_switch_action(
    const fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP>& i_target,
    const bool i_switch_ab,
    const bool i_switch_cd)
{
    FAPI_DBG("Start");
    fapi2::buffer<uint64_t> pmisc_data;
    fapi2::buffer<uint64_t> pmisc_mask;

    // Build ADU pMisc Mode register content
    FAPI_DBG("writing ADU pMisc Mode register: switch_ab=%s, switch_cd=%s",
             i_switch_ab ? "true" : "false", i_switch_cd ? "true" : "false");

    // Switch AB bit
    pmisc_data.writeBit<PU_SND_MODE_REG_ENABLE_PB_SWITCH_AB>(i_switch_ab);
    pmisc_mask.setBit<PU_SND_MODE_REG_ENABLE_PB_SWITCH_AB>();

    // Switch CD bit
    pmisc_data.writeBit<PU_SND_MODE_REG_ENABLE_PB_SWITCH_CD>(i_switch_cd);
    pmisc_mask.setBit<PU_SND_MODE_REG_ENABLE_PB_SWITCH_CD>();

    FAPI_TRY(fapi2::putScomUnderMask(i_target, PU_SND_MODE_REG, pmisc_data, pmisc_mask),
             "Error from putScomUnderMask (PU_SND_MODE_REG)");

fapi_try_exit:
    FAPI_DBG("End");
    return fapi2::current_err;
}


// NOTE: see comments above function prototype in header
fapi2::ReturnCode p9_build_smp_sequence_adu(p9_build_smp_system& i_smp,
        const p9_build_smp_operation i_op,
        const p9_build_smp_adu_action i_action)
{
    FAPI_DBG("Start");
    fapi2::ReturnCode l_rc;
    bool l_lock_pick = (i_op == SMP_ACTIVATE_PHASE1) ?
                       P9_BUILD_SMP_PHASE1_ADU_PICK_LOCK :
                       P9_BUILD_SMP_PHASE2_ADU_PICK_LOCK;
    uint8_t l_num_attempts = (i_op == SMP_ACTIVATE_PHASE1) ?
                             P9_BUILD_SMP_PHASE1_ADU_LOCK_ATTEMPTS :
                             P9_BUILD_SMP_PHASE2_ADU_LOCK_ATTEMPTS;
    bool l_adu_is_dirty = false;

    // validate input action, set ADU operation parameters
    p9_ADU_oper_flag l_adu_oper_flag;

    switch (i_action)
    {
        case SWITCH_AB:
        case SWITCH_CD:
            l_adu_oper_flag.setOperationType(p9_ADU_oper_flag::PMISC_OPER);
            l_adu_oper_flag.setTransactionSize(p9_ADU_oper_flag::TSIZE_1);
            break;

        case QUIESCE:
            l_adu_oper_flag.setOperationType(p9_ADU_oper_flag::PB_DIS_OPER);
            break;

        default:
            FAPI_ASSERT(false,
                        fapi2::P9_BUILD_SMP_BAD_ADU_ACTION_ERR()
                        .set_OP(i_op)
                        .set_ACTION(i_action),
                        "Invalid ADU action specified");
    }

    // loop through all chips, lock & reset ADU
    FAPI_DBG("Acquiring lock for all ADU units in drawer");

    for (auto g_iter = i_smp.groups.begin();
         g_iter != i_smp.groups.end();
         ++g_iter)
    {
        for (auto p_iter = g_iter->second.chips.begin();
             p_iter != g_iter->second.chips.end();
             ++p_iter)
        {
            // acquire ADU lock
            l_rc = p9_adu_coherent_manage_lock(
                       *(p_iter->second.target),
                       l_lock_pick,
                       true,           // Acquire lock
                       l_num_attempts);

            if (l_rc)
            {
                FAPI_ERR("Error from p9_adu_coherent_manage_lock (acquire all)");

                if (l_adu_is_dirty)
                {
                    goto adu_reset_unlock;
                }
                else
                {
                    fapi2::current_err = l_rc;
                    goto fapi_try_exit;
                }
            }

            // NOTE: lock is now held, if an operation fails from this point
            //       to the end of the procedure:
            //         o attempt to cleanup/release lock (so that procedure does
            //         not leave the ADU in a locked state)
            //         o return rc of original fail
            l_adu_is_dirty = true;

            // Reset ADU
            l_rc = p9_adu_coherent_utils_reset_adu(*(p_iter->second.target));

            if (l_rc)
            {
                FAPI_ERR("Error from p9_adu_coherent_utils_reset_adu (acquire all)");
                goto adu_reset_unlock;
            }

            // Condition for hotplug switch operation
            // all chips which were not quiesced prior to switch AB will
            // need to observe the switch
            if (i_action != QUIESCE)
            {
                l_rc = p9_build_smp_adu_set_switch_action(
                           *(p_iter->second.target),
                           (i_action == SWITCH_AB),
                           (i_action == SWITCH_CD));

                if (l_rc)
                {
                    FAPI_ERR("Error from p9_build_smp_adu_set_switch_action (acquire all)");
                    goto adu_reset_unlock;
                }
            }
        }
    }

    // perform action on specified chips
    FAPI_DBG("All ADU locks now held, performing action = %d", i_action);

    for (auto g_iter = i_smp.groups.begin();
         g_iter != i_smp.groups.end();
         ++g_iter)
    {
        for (auto p_iter = g_iter->second.chips.begin();
             p_iter != g_iter->second.chips.end();
             ++p_iter)
        {
            if (((i_action == QUIESCE) &&
                 (p_iter->second.issue_quiesce_next)) ||
                ((i_action == SWITCH_AB) &&
                 p_iter->second.master_chip_sys_next) ||
                (i_action == SWITCH_CD))
            {
                char l_target_str[fapi2::MAX_ECMD_STRING_LEN];
                fapi2::toString(*(p_iter->second.target), l_target_str,
                                sizeof(l_target_str));
                FAPI_INF("Issuing ADU op for %s", l_target_str);

                // issue operation
                l_rc = p9_adu_coherent_setup_adu(*(p_iter->second.target),
                                                 0,
                                                 false,      // write
                                                 l_adu_oper_flag.setFlag());

                if (l_rc)
                {
                    FAPI_ERR("Error from p9_adu_coherent_setup_adu (op)");
                    goto adu_reset_unlock;
                }

                // Check status
                l_rc = p9_build_smp_adu_check_status(
                           *(p_iter->second.target),
                           i_smp);

                if (l_rc)
                {
                    FAPI_ERR("Error from p9_build_smp_adu_check_status (op)");
                    goto adu_reset_unlock;
                }

                // workaround for HW397129 to re-enable fastpath for DD1
                uint8_t l_hw397129_workaround;
                FAPI_TRY(FAPI_ATTR_GET(fapi2::ATTR_CHIP_EC_FEATURE_HW397129, *(p_iter->second.target), l_hw397129_workaround),
                         "Error from FAPI_ATTR_GET (ATTR_CHIP_EC_FEATURE_HW397129)");

                if ((i_action == SWITCH_AB) && l_hw397129_workaround)
                {
                    p9_ADU_oper_flag l_adu_oper_flag_reinit;
                    l_adu_oper_flag_reinit.setOperationType(p9_ADU_oper_flag::PB_INIT_OPER);
                    l_adu_oper_flag_reinit.setTransactionSize(p9_ADU_oper_flag::TSIZE_1);

                    FAPI_INF("Issuing re-init for HW397129");
                    l_rc = p9_adu_coherent_setup_adu(*(p_iter->second.target),
                                                     0,
                                                     false,      // write
                                                     l_adu_oper_flag_reinit.setFlag());

                    if (l_rc)
                    {
                        FAPI_ERR("Error from p9_adu_coherent_setup_adu (op, reinit)");
                        goto adu_reset_unlock;
                    }

                    // Check status
                    l_rc = p9_build_smp_adu_check_status(*(p_iter->second.target),
                                                         i_smp);

                    if (l_rc)
                    {
                        FAPI_ERR("Error from p9_build_smp_adu_check_status (op, reinit)");
                        goto adu_reset_unlock;
                    }
                }
            }
        }
    }

    // loop through all chips, reset switch controls and unlock ADUs
    FAPI_DBG("Operation complete, releasing lock for all ADU units in drawer");

    for (auto g_iter = i_smp.groups.begin();
         g_iter != i_smp.groups.end();
         ++g_iter)
    {
        for (auto p_iter = g_iter->second.chips.begin();
             p_iter != g_iter->second.chips.end();
             ++p_iter)
        {
            // reset switch controls
            if (i_action != QUIESCE)
            {
                l_rc = p9_build_smp_adu_set_switch_action(
                           *(p_iter->second.target),
                           false,
                           false);

                if (l_rc)
                {
                    FAPI_ERR("Error from p9_build_smp_adu_set_switch_action (release all)");
                    goto adu_reset_unlock;
                }
            }

            // release ADU lock
            l_lock_pick = false;
            l_rc = p9_adu_coherent_manage_lock(*(p_iter->second.target),
                                               l_lock_pick,
                                               false, // Release lock
                                               l_num_attempts);

            if (l_rc)
            {
                FAPI_ERR("Error from p9_adu_coherent_manage_lock (release all)");
                goto adu_reset_unlock;
            }
        }
    }

    FAPI_DBG("All ADU locks released");
    // no error for entire operation
    l_adu_is_dirty = false;

adu_reset_unlock:

    // if error has occurred and any ADU is dirty,
    // attempt to reset all ADUs and free locks (propogate rc of original fail)
    if (l_rc && l_adu_is_dirty)
    {
        // save original error for return
        fapi2::current_err = l_rc;

        FAPI_INF("Attempting to reset/free lock on all ADUs");

        // loop through all chips, unlock ADUs
        for (auto g_iter = i_smp.groups.begin();
             g_iter != i_smp.groups.end();
             ++g_iter)
        {
            for (auto p_iter = g_iter->second.chips.begin();
                 p_iter != g_iter->second.chips.end();
                 ++p_iter)
            {
                // ignore return codes
                l_rc = p9_adu_coherent_utils_reset_adu(
                           *(p_iter->second.target));
                l_rc = p9_adu_coherent_manage_lock(
                           *(p_iter->second.target),
                           false, // No lock pick
                           false, // Lock release
                           1);    // Attempt 1 time
            }
        }
    }

fapi_try_exit:
    FAPI_DBG("End");
    return fapi2::current_err;
}
