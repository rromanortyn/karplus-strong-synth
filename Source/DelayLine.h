/*
  ==============================================================================

    DelayLine.h
    Created: 2026 April
    For Audio Programming Assignment 3

  ==============================================================================
*/

#pragma once

#include <vector>
#include <cmath>

/**
 * Fractional delay line using a circular buffer.
 *
 * This class only manages the delay buffer itself:
 * - setting a maximum delay size
 * - setting a fractional delay time
 * - reading from the delayed position using linear interpolation
 * - writing a new sample into the buffer
 * - advancing the read/write positions
 *
 * Feedback is intentionally not handled inside this class. For the
 * Karplus-Strong model, feedback, damping and decay will be implemented
 * in a separate KarplusStrongString class.
 */
class DelayLine
{
public:

    /**
     * Set the maximum delay size in samples and allocate the circular buffer.
     *
     * @param _maxSizeInSamples maximum delay size in samples
     */
    void setMaximumDelaySize(int _maxSizeInSamples)
    {
        if (_maxSizeInSamples < 2)
            _maxSizeInSamples = 2;

        maximumBufferSize = _maxSizeInSamples;

        circularBuffer.clear();
        circularBuffer.resize(maximumBufferSize, 0.0f);

        writePos = 0;
        delayTimeInSamples = 1.0f;

        updateReadPosition();
    }

    /**
     * Clear the delay buffer and reset the read/write positions.
     */
    void clear()
    {
        std::fill(circularBuffer.begin(), circularBuffer.end(), 0.0f);

        writePos = 0;
        updateReadPosition();
    }

    /**
     * Set the delay time in samples.
     *
     * This can be fractional, which is useful for more accurate pitch
     * tuning in the Karplus-Strong string model.
     *
     * @param _delayTimeInSamples delay time in samples
     */
    void setDelayTimeInSamples(float _delayTimeInSamples)
    {
        if (_delayTimeInSamples < 1.0f)
            _delayTimeInSamples = 1.0f;

        if (_delayTimeInSamples >= static_cast<float>(maximumBufferSize - 1))
            _delayTimeInSamples = static_cast<float>(maximumBufferSize - 1);

        delayTimeInSamples = _delayTimeInSamples;

        updateReadPosition();
    }

    /**
     * Read the delayed sample using linear interpolation.
     *
     * @return delayed sample
     */
    float read() const
    {
        if (maximumBufferSize <= 0)
            return 0.0f;

        const int indexA = static_cast<int>(std::floor(readPos));

        int indexB = indexA + 1;

        if (indexB >= maximumBufferSize)
            indexB = 0;

        const float sampleA = circularBuffer[indexA];
        const float sampleB = circularBuffer[indexB];

        const float fraction = readPos - static_cast<float>(indexA);

        return sampleA + fraction * (sampleB - sampleA);
    }

    /**
     * Write one sample into the delay line at the current write position.
     *
     * @param inputSample sample to write into the circular buffer
     */
    void write(float inputSample)
    {
        if (maximumBufferSize <= 0)
            return;

        circularBuffer[writePos] = inputSample;
    }

    /**
     * Advance both read and write positions by one sample.
     */
    void advance()
    {
        readPos += 1.0f;
        writePos += 1;

        if (readPos >= static_cast<float>(maximumBufferSize))
            readPos -= static_cast<float>(maximumBufferSize);

        if (writePos >= maximumBufferSize)
            writePos = 0;
    }

    /**
     * Convenience function:
     * read delayed sample, write new sample, then advance positions.
     *
     * This is useful for simple delay-line use, but in Karplus-Strong we may
     * still prefer to call read(), write(), and advance() separately for clarity.
     *
     * @param inputSample sample to write into the delay line
     * @return delayed sample read before writing
     */
    float process(float inputSample)
    {
        const float delayedSample = read();

        write(inputSample);
        advance();

        return delayedSample;
    }

private:

    /**
     * Recalculate the read position from the write position and delay time.
     */
    void updateReadPosition()
    {
        readPos = static_cast<float>(writePos) - delayTimeInSamples;

        while (readPos < 0.0f)
            readPos += static_cast<float>(maximumBufferSize);

        while (readPos >= static_cast<float>(maximumBufferSize))
            readPos -= static_cast<float>(maximumBufferSize);
    }

    std::vector<float> circularBuffer;

    float delayTimeInSamples = 1.0f;
    float readPos = 0.0f;

    int maximumBufferSize = 2;
    int writePos = 0;
};