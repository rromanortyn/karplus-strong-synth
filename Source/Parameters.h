#pragma once
struct StringParameters
{
    float damping = 0.6f, exciteAmount = 0.7f, decay = 0.85f;
    float impulseWidth = 0.5f, brightness = 1.0f, pickPosition = 0.5f;
    float body = 0.0f, stereoWidth = 0.0f, detune = 0.0f;
    int exciterType = 0, interpolation = 0, pitchTransition = 0;
};
