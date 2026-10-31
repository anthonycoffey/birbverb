# Offline DSP test suite — SPEC-006, ADR-004.
#
# birbverb_tests links Catch2 and bv_dsp. It does NOT link JUCE, and that is the
# point: it is a link-time guarantee that Source/dsp/ stays JUCE-free.

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1          # pinned to a tag, never a branch — see ADR-004
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(birbverb_tests
    test_measure.cpp
    test_stability.cpp
    test_dispersion_curve.cpp
    test_dispersive_allpass.cpp
    test_spring_line.cpp
    test_dispersion.cpp
    test_decay.cpp)

target_include_directories(birbverb_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(birbverb_tests PRIVATE bv_dsp Catch2::Catch2WithMain)

# Float behaviour has to be predictable for measurements to be deterministic
# (SPEC-006 requirement 4), so no fast-math on this target.
if(MSVC)
    target_compile_options(birbverb_tests PRIVATE /fp:precise)
else()
    target_compile_options(birbverb_tests PRIVATE -fno-fast-math)
endif()

# Two CTest entries, because the suite is deliberately part-RED.
#
#   implemented  — everything that should pass right now. CI gates on this.
#   red          — tests written ahead of their implementation, tagged [.red] in
#                  Catch2 so they are hidden from the default run. The TDD loop is:
#                  write the test tagged [.red], watch it fail here, implement it,
#                  then drop the leading '.' so it joins the default suite.
#
#                  There are currently NO red tests: SPEC-001's were all
#                  promoted when SpringTank landed. Catch2 exits non-zero when a
#                  filter matches nothing, so WILL_FAIL keeps this entry green
#                  while it is empty, and it regains its real meaning the moment
#                  the next [.red] test is written. That is why it is left in
#                  place rather than deleted.
add_test(NAME implemented COMMAND birbverb_tests)
add_test(NAME red COMMAND birbverb_tests "[red]")

set_tests_properties(red PROPERTIES LABELS "red" WILL_FAIL TRUE)
set_tests_properties(implemented PROPERTIES LABELS "implemented")
