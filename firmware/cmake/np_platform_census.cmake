# NeurOne SW-02 platform-symbol census
# Document: NP-SW-CI-001 §4.8
#
# Run as `cmake -DNM=... -DOBJECT=... -DEXPECTED=... -P np_platform_census.cmake`
# from a POST_BUILD step on np_platform_stub_objs.
#
# Asserts that the number of global symbols firmware/platform/src/np_platform_stub.c
# defines equals NP_SW02_PLATFORM_SYMBOL_COUNT, which is parsed out of
# np_sw02_platform_hal.h at configure time rather than repeated here.
#
# Why this exists.  The count is the measure of how far SW-02 is from a runnable
# image, and a measure nothing checks is a comment.  Without this step, adding a
# platform seam and a matching trap would move the real number and leave the
# published one behind, which is how §4.7's vacuous-metric family starts.  With
# it, the number can only change by editing the header, and editing the header
# is a reviewable line in a diff.
#
# It is deliberately an equality, not a ceiling: a trap DISAPPEARING without the
# constant moving means either a real driver landed (progress, and it should be
# recorded) or a symbol was dropped from the contract (a regression).  Both are
# events; neither should pass silently.

if(NOT NM OR NOT OBJECT OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "np_platform_census: NM, OBJECT and EXPECTED are all required")
endif()

if(NOT EXISTS "${OBJECT}")
    message(FATAL_ERROR
        "np_platform_census: object not found: ${OBJECT}\n"
        "This step must never be able to pass by finding nothing to look at — "
        "that is the failure NP-SW-CI-001 §6.6/§4.4.2 records for the "
        "`find … | xargs -r` image-size step.")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --extern-only "${OBJECT}"
    OUTPUT_VARIABLE _nm_out
    RESULT_VARIABLE _nm_rc
    ERROR_VARIABLE  _nm_err)

if(NOT _nm_rc EQUAL 0)
    message(FATAL_ERROR "np_platform_census: nm failed (${_nm_rc}): ${_nm_err}")
endif()

string(REPLACE "\n" ";" _lines "${_nm_out}")
set(_count 0)
set(_names "")
foreach(_line IN LISTS _lines)
    # "<addr> T <name>" — text symbols only.  A stub that became data would not
    # be a stub.
    if(_line MATCHES "^[0-9a-fA-F]+[ \t]+[Tt][ \t]+([A-Za-z_][A-Za-z0-9_]*)$")
        math(EXPR _count "${_count} + 1")
        list(APPEND _names "${CMAKE_MATCH_1}")
    endif()
endforeach()

if(NOT _count EQUAL EXPECTED)
    list(SORT _names)
    string(REPLACE ";" "\n  " _pretty "${_names}")
    message(FATAL_ERROR
        "np_platform_census: SW-02 platform trap count is ${_count}, "
        "NP_SW02_PLATFORM_SYMBOL_COUNT says ${EXPECTED}.\n"
        "A platform seam appeared, disappeared, or a driver replaced a trap. "
        "All three are design events: update the constant in "
        "firmware/platform/include/np_sw02_platform_hal.h and say why in "
        "NP-SW-CI-001 §4.8.\n"
        "Defined in np_platform_stub.c:\n  ${_pretty}")
endif()

message(STATUS
    "SW-02 platform census: ${_count} seams trapped, 0 driven "
    "(NP-SW-CI-001 §4.8)")
