// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NoiseParameter.generated.h"

USTRUCT(BlueprintType)
struct FNoiseParameter
{
    GENERATED_BODY()

    // --- General Settings ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|General", meta = (ToolTip = "The seed for the random number generator. The same seed will always produce the same noise pattern."))
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|General", meta = (ClampMin = "0.0", ToolTip = "Controls the frequency or 'zoom' of the noise. Higher values result in smaller, more detailed patterns."))
    float Frequency = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|General", meta = (ToolTip = "If true, the noise will output continuous values (e.g., -1.0 to 1.0). If false, it will be a binary output (-1 or 1) based on the Threshold."))
    bool bIsAnalog = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|General", meta = (ClampMin = "-1.0", ClampMax = "1.0", EditCondition = "!bIsAnalog", ToolTip = "The cutoff value for binary output. If the noise value is above this, the result is 1; otherwise, it is -1. Only used when 'Is Analog' is false."))
    float Threshold = 0.0f;

    // --- Fractal Settings ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|Fractal", meta = (ClampMin = "1", UIMax = "4", ToolTip = "The number of noise layers to stack (fractal iterations). Higher values add more detail but increase computation cost. Values above 4 rarely provide significant visual improvement."))
    int32 Octaves = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|Fractal", meta = (ClampMin = "1.0", EditCondition = "Octaves > 1", ToolTip = "The frequency multiplier for each successive octave. A value of 2.0 means each octave is twice the frequency of the previous one."))
    float Lacunarity = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|Fractal", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Octaves > 1", ToolTip = "The amplitude multiplier for each successive octave (also called Persistence). A value of 0.5 means each octave's contribution is half of the previous one."))
    float Gain = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise Settings|Fractal", meta = (ClampMin = "0.0", UIMax = "1.0", EditCondition = "Octaves > 1", ToolTip = "This controls the strength of the weighting effect. A value of 0.0 means no weighting."))
    float WeightedStrength = 0.0f;
};