/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/isteps/istep13/call_mss_scominit.C $                  */
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

//Error handling and tracing
#include <errl/errlentry.H>
#include <errl/errlmanager.H>
#include <errl/errludtarget.H>
#include <isteps/hwpisteperror.H>
#include <initservice/isteps_trace.H>

//  targeting support
#include    <targeting/common/commontargeting.H>
#include    <targeting/common/util.H>
#include    <targeting/common/utilFilter.H>

// fapi2 HWP invoker
#include    <fapi2/plat_hwp_invoker.H>

//From Import Directory (EKB Repository)
#include    <config.h>
#include    <fapi2.H>
#include    <p9_mss_scominit.H>
#include    <p9_throttle_sync.H>
#include    <p9c_mss_scominit.H>

using   namespace   ERRORLOG;
using   namespace   ISTEP;
using   namespace   ISTEP_ERROR;
using   namespace   TARGETING;

namespace ISTEP_13
{
void* call_mss_scominit (void *io_pArgs)
{
    errlHndl_t l_err = NULL;

    IStepError l_stepError;

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace, "call_mss_scominit entry" );

    do
    {
        // Get all MCBIST targets
        TARGETING::TargetHandleList l_mcbistTargetList;
        getAllChiplets(l_mcbistTargetList, TYPE_MCBIST);

        for (const auto & l_target : l_mcbistTargetList)
        {
            // Dump current run on target
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                    "Running p9_mss_scominit HWP on "
                    "target HUID %.8X",
                    TARGETING::get_huid(l_target));

            fapi2::Target <fapi2::TARGET_TYPE_MCBIST> l_fapi_target
                (l_target);

            //  call the HWP with each fapi2::Target
            FAPI_INVOKE_HWP(l_err, p9_mss_scominit, l_fapi_target);

            if (l_err)
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          "ERROR 0x%.8X: p9_mss_scominit HWP returns error",
                          l_err->reasonCode());

                // capture the target data in the elog
                ErrlUserDetailsTarget(l_target).addToLog(l_err);

                // Create IStep error log and cross reference to error that
                // occurred
                l_stepError.addErrorDetails( l_err );

                // Commit Error
                errlCommit( l_err, HWPF_COMP_ID );

                break;
            }
            else
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "SUCCESS running p9_mss_scominit HWP on "
                           "target HUID %.8X", TARGETING::get_huid(l_target));
            }
        }

        if (!l_stepError.isNull())
        {
            break;
        }

         // Get all MBA targets
        TARGETING::TargetHandleList l_membufTargetList;
        getAllChips(l_membufTargetList, TYPE_MEMBUF);

        for (const auto & l_membuf_target : l_membufTargetList)
        {
            // Dump current run on target
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                    "Running p9c_mss_scominit HWP on "
                    "target HUID %.8X",
                    TARGETING::get_huid(l_membuf_target));

            fapi2::Target <fapi2::TARGET_TYPE_MEMBUF_CHIP> l_fapi_membuf_target
                (l_membuf_target);

            //  call the HWP with each fapi2::Target
            FAPI_INVOKE_HWP(l_err, p9c_mss_scominit, l_fapi_membuf_target);

            if (l_err)
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          "ERROR 0x%.8X: p9c_mss_scominit HWP returns error",
                          l_err->reasonCode());

                // capture the target data in the elog
                ErrlUserDetailsTarget(l_membuf_target).addToLog(l_err);

                // Create IStep error log and cross reference to error that
                // occurred
                l_stepError.addErrorDetails( l_err );

                // Commit Error
                errlCommit( l_err, HWPF_COMP_ID );

                break;
            }
            else
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                      "SUCCESS running p9c_mss_scominit HWP on "
                      "target HUID %.8X", TARGETING::get_huid(l_membuf_target));
            }
        }

        if (!l_stepError.isNull())
        {
            break;
        }

        // Run proc throttle sync
        // Get all functional proc chip targets
        TARGETING::TargetHandleList l_cpuTargetList;
        getAllChips(l_cpuTargetList, TYPE_PROC);

        for (const auto & l_procChip: l_cpuTargetList)
        {
            const fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP>
                l_fapi_cpu_target(l_procChip);

            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   "Running p9_throttle_sync HWP on "
                   "target HUID %.8X", TARGETING::get_huid(l_procChip));

            // Call proc_throttle_sync
            FAPI_INVOKE_HWP( l_err, p9_throttle_sync, l_fapi_cpu_target );

            if (l_err)
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          "ERROR 0x%.8X: p9_throttle_sync HWP returns error",
                          l_err->reasonCode());

                // Capture the target data in the elog
                ErrlUserDetailsTarget(l_procChip).addToLog(l_err);

                // Create IStep error log and cross reference
                // to error that occurred
                l_stepError.addErrorDetails( l_err );

                // Commit Error
                errlCommit( l_err, HWPF_COMP_ID );

                break;
            }
            else
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "SUCCESS running p9_throttle_sync HWP on "
                           "target HUID %.8X", TARGETING::get_huid(l_procChip));
            }
        }

    } while (0);

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace, "call_mss_scominit exit" );
    return l_stepError.getErrorHandle();
}

};
