#pragma once
#include <JuceHeader.h>
#include <array>

// Single audio producer, single GUI consumer. Full queues drop visual samples;
// they never stall audio, and the producer never touches the consumer's memory.
class ScopeFifo
{
public:
    void reset() noexcept { fifo.reset(); }
    void push(const float* input, int count) noexcept
    {
        if (fifo.getFreeSpace() < count) return;
        int a, n, b, m; fifo.prepareToWrite(count, a, n, b, m);
        std::copy_n(input, n, data.begin() + a);
        std::copy_n(input + n, m, data.begin() + b);
        fifo.finishedWrite(n + m);
    }
    int read(float* output, int maximum) noexcept
    {
        int a, n, b, m; fifo.prepareToRead(maximum, a, n, b, m);
        std::copy_n(data.begin() + a, n, output);
        std::copy_n(data.begin() + b, m, output + n);
        fifo.finishedRead(n + m); return n + m;
    }
private:
    std::array<float, 8192> data {};
    juce::AbstractFifo fifo { 8192 };
};
