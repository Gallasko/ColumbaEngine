# Build-time wrapper for automatic VM op-kernel regeneration.
#
# Invoked as:
#   cmake -DSTAGE0=<path> -DTOOLS_DIR=<path> -DGEN_DIR=<path> -DSTAMP=<path>
#         -P regen_vm_ops_stage0.cmake
#
# Runs the stage-0 PgCompilerBootstrap (a copy persisted by the previous
# build) to regenerate src/Engine/Compiler/generated/*.inc from the spec.
# On a clean build directory no stage-0 compiler exists yet; the committed
# generated files are used as the bootstrap seed and this script is a no-op
# (check_vm_ops_fresh.sh guards against that seed being stale).

if(EXISTS "${STAGE0}")
    execute_process(
        COMMAND "${STAGE0}" gen_vm_ops.pg "${GEN_DIR}"
        WORKING_DIRECTORY "${TOOLS_DIR}"
        RESULT_VARIABLE rc
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "stage-0 VM ops regeneration failed (rc=${rc}): ${STAGE0} gen_vm_ops.pg ${GEN_DIR}")
    endif()
    message(STATUS "VM op kernels regenerated from tools/vm_ops_def.pg (stage-0)")
else()
    message(STATUS "No stage-0 PgCompilerBootstrap yet — using committed generated VM op files")
endif()

get_filename_component(stamp_dir "${STAMP}" DIRECTORY)
file(MAKE_DIRECTORY "${stamp_dir}")
file(TOUCH "${STAMP}")
