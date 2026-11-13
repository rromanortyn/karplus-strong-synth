#pragma once
#include <complex>
#include <cmath>
#include <algorithm>

struct StringTuning
{
    double delay = 1.0, coefficient = 0.0, radius = 1.0;

    // -arg(H(exp(j*omega))) / omega, in samples. Pitch compensation needs
    // phase delay, not group delay; both tend to (1-alpha)/alpha at DC.
    static double filterPhaseDelay(double omega, double alpha) noexcept
    {
        const double b = 1.0 - alpha;
        if (omega == 0.0) return b / alpha;
        return std::atan2(b * std::sin(omega), 1.0 - b * std::cos(omega)) / omega;
    }

    // Calibrate the damped closed-loop pole, not just its unit-circle phase.
    // Linear interpolation has loss, so its pole radius also affects tuning.
    // Only called at note/parameter changes; no allocation or per-sample search.
    static StringTuning calculate(double omega, double alpha, double gain, int mode) noexcept
    {
        using Complex = std::complex<double>;
        const double b = 1.0 - alpha;
        const double ideal = 6.283185307179586 / omega - filterPhaseDelay(omega, alpha);
        const int base = static_cast<int>(std::floor(ideal));
        const Complex unit = std::polar(1.0, -omega);
        int integer = base;
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            if (integer < 1) break;
            const Complex phase = std::polar(gain * alpha, -integer * omega);
            auto evaluate = [&](double logarithm)
            {
                const Complex q = std::exp(-logarithm) * unit;
                const Complex a = std::exp(-integer * logarithm) * phase / (1.0 - b * q);
                if (mode == 0)
                {
                    const Complex d = a * (q - 1.0);
                    const double fraction = -a.imag() / d.imag();
                    return std::pair<double, double> { (a + fraction * d).real() - 1.0, fraction };
                }
                const Complex coefficient = (1.0 - a * q) / (a - q);
                return std::pair<double, double> { coefficient.imag(), coefficient.real() };
            };
            const double bottom = -std::min(4.0, 20.0 / integer);
            double high = 0.0, highValue = evaluate(high).first;
            const int steps = 32;
            int adjustment = 0;
            for (int step = 1; step <= steps; ++step)
            {
                double low = bottom * step / steps;
                const double lowValue = evaluate(low).first;
                if (lowValue * highValue <= 0.0)
                {
                    const double savedHigh = high;
                    for (int iteration = 0; iteration < 52; ++iteration)
                    {
                        const double middle = (low + high) * 0.5;
                        if (evaluate(middle).first * evaluate(high).first > 0.0) high = middle;
                        else low = middle;
                    }
                    const double logarithm = (low + high) * 0.5;
                    const auto result = evaluate(logarithm);
                    if (std::abs(result.first) < 1.0e-6 && result.second >= 0.0 && result.second <= 1.0)
                    {
                        if (mode == 0) return { integer + result.second, 0.0, std::exp(logarithm) };
                        // delay's fraction controls bypass detection and visual
                        // length; the calibrated allpass coefficient controls DSP.
                        const double theta = -std::arg((result.second + unit) / (1.0 + result.second * unit));
                        return { integer + theta / omega, result.second, std::exp(logarithm) };
                    }
                    if (std::abs(result.first) < 1.0e-6 && std::isfinite(result.second))
                    {
                        const double equivalentFraction = mode == 0 ? result.second
                            : -std::arg((result.second + unit) / (1.0 + result.second * unit)) / omega;
                        adjustment = static_cast<int>(std::floor(equivalentFraction));
                        adjustment = std::clamp(adjustment, -32, 32);
                        break;
                    }
                    high = savedHigh;
                }
                high = bottom * step / steps; highValue = lowValue;
            }
            if (adjustment == 0) break;
            integer += adjustment;
        }
        // Finite, stable fallback for unsupported rates or pathological inputs.
        const double fraction = ideal - base;
        const double phase = omega * fraction;
        return { mode == 0 ? base + std::sin(phase) / (std::sin(omega - phase) + std::sin(phase)) : ideal,
                 std::sin(0.5 * (omega - phase)) / std::sin(0.5 * (omega + phase)), 1.0 };
    }
};
