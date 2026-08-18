// Copyright Epic Games, Inc. All Rights Reserved.
// Modified from UE original source code

#pragma once

#include "CoreMinimal.h"

struct FNoiseParameter;

class DESTRUCTIBLECAVEGENERATOR_API FNoiseMathUtility
{
public:
	FNoiseMathUtility(int InSeed, float InFrequency, bool InIsAnalog, float InThreshold, int InOctaves, float InLacunarity, float InGain, float InWeightedStrength);
	FNoiseMathUtility(const FNoiseParameter& InParam);
	
	void SetSeed(int InSeed);
	void CalculateFractalBounding();

	/**
	* Generates a 2D Perlin noise sample at the given location.  Returns a continuous random value between -1.0 and 1.0.
	*
	* @param	Location	Where to sample
	*
	* @return	Perlin noise in the range of -1.0 to 1.0
	*/
	float PerlinNoise2D(const FVector2D& Location);
	
	float ModifiedPerlinNoise2D(const FVector2D& Location);

	/**
	* Generates a 3D Perlin noise sample at the given location.  Returns a continuous random value between -1.0 and 1.0.
	*
	* @param	Location	Where to sample
	*
	* @return	Perlin noise in the range of -1.0 to 1.0
	*/
	float PerlinNoise3D(const FVector& Location);

	float ModifiedPerlinNoise3D(const FVector& Location);

private:
	int Seed;
	float Frequency;
	bool bIsAnalog;
	float Threshold;
	int Octaves;
	float Lacunarity;
	float Gain;
	float WeightedStrength;

	float FractalBounding = 1 / 1.75f;
};
