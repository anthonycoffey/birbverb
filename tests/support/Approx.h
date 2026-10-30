#pragma once

// Tolerance matchers — SPEC-006 requirements 6 and the Tolerance helpers design.
//
// The whole reason this file exists: a DSP test failure has to print the measured
// value, the expected value and the tolerance range. "Assertion failed" tells you
// nothing when the thing being asserted is a number with a ±15% window, and a
// suite whose failures you cannot read is a suite you stop running.

#include <catch2/matchers/catch_matchers_templated.hpp>
#include <cmath>
#include <sstream>
#include <string>

namespace bv::test
{

/** Wrapper so `Percent(15.0)` reads differently from an absolute tolerance. */
struct Percent
{
    explicit Percent (double v) : value (v) {}
    double value;
};

//==============================================================================
namespace detail
{
    inline std::string fmt (double v, int places = 3)
    {
        std::ostringstream os;
        os.setf (std::ios::fixed);
        os.precision (places);
        os << v;
        return os.str();
    }

    /** Shared implementation: a value within [expected-tol, expected+tol], with a
        failure message naming the unit and the resulting window. */
    struct WithinUnit : Catch::Matchers::MatcherGenericBase
    {
        WithinUnit (double expectedIn, double toleranceIn, std::string unitIn,
                    bool relativeIn, int placesIn)
            : expected (expectedIn), tolerance (toleranceIn),
              unit (std::move (unitIn)), relative (relativeIn), places (placesIn) {}

        double absTolerance() const
        {
            return relative ? std::abs (expected) * tolerance / 100.0 : tolerance;
        }

        bool match (double measured) const
        {
            if (! std::isfinite (measured)) return false;
            return std::abs (measured - expected) <= absTolerance();
        }

        std::string describe() const override
        {
            const double t = absTolerance();
            std::ostringstream os;
            os << "is " << detail::fmt (expected, places) << " " << unit << " +/- ";
            if (relative)
                os << detail::fmt (tolerance, 1) << "% ";
            else
                os << detail::fmt (t, places) << " " << unit << " ";
            os << "(" << detail::fmt (expected - t, places) << ".."
               << detail::fmt (expected + t, places) << " " << unit << ")";
            return os.str();
        }

        double      expected;
        double      tolerance;
        std::string unit;
        bool        relative;
        int         places;
    };
} // namespace detail

//==============================================================================
// Milliseconds

inline detail::WithinUnit WithinMs (double expectedMs, double absToleranceMs)
{
    return { expectedMs, absToleranceMs, "ms", false, 1 };
}

inline detail::WithinUnit WithinMs (double expectedMs, Percent tolerance)
{
    return { expectedMs, tolerance.value, "ms", true, 1 };
}

// Hertz

inline detail::WithinUnit WithinHz (double expectedHz, double absToleranceHz)
{
    return { expectedHz, absToleranceHz, "Hz", false, 1 };
}

inline detail::WithinUnit WithinHz (double expectedHz, Percent tolerance)
{
    return { expectedHz, tolerance.value, "Hz", true, 1 };
}

// Decibels — always absolute. A "percentage tolerance in dB" is meaningless.

inline detail::WithinUnit WithinDb (double expectedDb, double absToleranceDb)
{
    return { expectedDb, absToleranceDb, "dB", false, 2 };
}

// Seconds

inline detail::WithinUnit WithinSeconds (double expectedS, Percent tolerance)
{
    return { expectedS, tolerance.value, "s", true, 3 };
}

inline detail::WithinUnit WithinSeconds (double expectedS, double absToleranceS)
{
    return { expectedS, absToleranceS, "s", false, 3 };
}

// Dimensionless ratio, for THD and similar

inline detail::WithinUnit WithinRatio (double expected, double absTolerance)
{
    return { expected, absTolerance, "", false, 4 };
}

} // namespace bv::test
