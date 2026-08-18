// Copyright Epic Games, Inc. All Rights Reserved.
// Modified from UE original source code

#include "Math/NoiseMathUtility.h"
#include "Math/NoiseParameter.h"

namespace FMathPerlinHelpers
{	
	// random permutation of 256 numbers, repeated 2x
	static int32 Permutation[512] = {
		63, 9, 212, 205, 31, 128, 72, 59, 137, 203, 195, 170, 181, 115, 165, 40, 116, 139, 175, 225, 132, 99, 222, 2, 41, 15, 197, 93, 169, 90, 228, 43, 221, 38, 206, 204, 73, 17, 97, 10, 96, 47, 32, 138, 136, 30, 219,
		78, 224, 13, 193, 88, 134, 211, 7, 112, 176, 19, 106, 83, 75, 217, 85, 0, 98, 140, 229, 80, 118, 151, 117, 251, 103, 242, 81, 238, 172, 82, 110, 4, 227, 77, 243, 46, 12, 189, 34, 188, 200, 161, 68, 76, 171, 194,
		57, 48, 247, 233, 51, 105, 5, 23, 42, 50, 216, 45, 239, 148, 249, 84, 70, 125, 108, 241, 62, 66, 64, 240, 173, 185, 250, 49, 6, 37, 26, 21, 244, 60, 223, 255, 16, 145, 27, 109, 58, 102, 142, 253, 120, 149, 160,
		124, 156, 79, 186, 135, 127, 14, 121, 22, 65, 54, 153, 91, 213, 174, 24, 252, 131, 192, 190, 202, 208, 35, 94, 231, 56, 95, 183, 163, 111, 147, 25, 67, 36, 92, 236, 71, 166, 1, 187, 100, 130, 143, 237, 178, 158,
		104, 184, 159, 177, 52, 214, 230, 119, 87, 114, 201, 179, 198, 3, 248, 182, 39, 11, 152, 196, 113, 20, 232, 69, 141, 207, 234, 53, 86, 180, 226, 74, 150, 218, 29, 133, 8, 44, 123, 28, 146, 89, 101, 154, 220, 126,
		155, 122, 210, 168, 254, 162, 129, 33, 18, 209, 61, 191, 199, 157, 245, 55, 164, 167, 215, 246, 144, 107, 235, 

		63, 9, 212, 205, 31, 128, 72, 59, 137, 203, 195, 170, 181, 115, 165, 40, 116, 139, 175, 225, 132, 99, 222, 2, 41, 15, 197, 93, 169, 90, 228, 43, 221, 38, 206, 204, 73, 17, 97, 10, 96, 47, 32, 138, 136, 30, 219,
		78, 224, 13, 193, 88, 134, 211, 7, 112, 176, 19, 106, 83, 75, 217, 85, 0, 98, 140, 229, 80, 118, 151, 117, 251, 103, 242, 81, 238, 172, 82, 110, 4, 227, 77, 243, 46, 12, 189, 34, 188, 200, 161, 68, 76, 171, 194,
		57, 48, 247, 233, 51, 105, 5, 23, 42, 50, 216, 45, 239, 148, 249, 84, 70, 125, 108, 241, 62, 66, 64, 240, 173, 185, 250, 49, 6, 37, 26, 21, 244, 60, 223, 255, 16, 145, 27, 109, 58, 102, 142, 253, 120, 149, 160,
		124, 156, 79, 186, 135, 127, 14, 121, 22, 65, 54, 153, 91, 213, 174, 24, 252, 131, 192, 190, 202, 208, 35, 94, 231, 56, 95, 183, 163, 111, 147, 25, 67, 36, 92, 236, 71, 166, 1, 187, 100, 130, 143, 237, 178, 158,
		104, 184, 159, 177, 52, 214, 230, 119, 87, 114, 201, 179, 198, 3, 248, 182, 39, 11, 152, 196, 113, 20, 232, 69, 141, 207, 234, 53, 86, 180, 226, 74, 150, 218, 29, 133, 8, 44, 123, 28, 146, 89, 101, 154, 220, 126,
		155, 122, 210, 168, 254, 162, 129, 33, 18, 209, 61, 191, 199, 157, 245, 55, 164, 167, 215, 246, 144, 107, 235
	};
	
	// Gradient functions for 1D, 2D and 3D Perlin noise

	FORCEINLINE float Grad1(int32 Hash, float X)
	{
		// Slicing Perlin's 3D improved noise would give us only scales of -1, 0 and 1; this looks pretty bad so let's use a different sampling
		static const float Grad1Scales[16] = {-8/8, -7/8., -6/8., -5/8., -4/8., -3/8., -2/8., -1/8., 1/8., 2/8., 3/8., 4/8., 5/8., 6/8., 7/8., 8/8};
		return Grad1Scales[Hash & 15] * X;
	}

	// Note: If you change the Grad2 or Grad3 functions, check that you don't change the range of the resulting noise as well; it should be (within floating point error) in the range of (-1, 1)
	FORCEINLINE float Grad2(int32 Hash, float X, float Y)
	{
		// corners and major axes (similar to the z=0 projection of the cube-edge-midpoint sampling from improved Perlin noise)
		switch (Hash & 7)
		{
		case 0: return X;
		case 1: return X + Y;
		case 2: return Y;
		case 3: return -X + Y;
		case 4: return -X;
		case 5: return -X - Y;
		case 6: return -Y;
		case 7: return X - Y;
			// can't happen
		default: return 0;
		}
	}

	FORCEINLINE float Grad3(int32 Hash, float X, float Y, float Z)
	{
		switch (Hash & 15)
		{
			// 12 cube midpoints
		case 0: return X + Z;
		case 1: return X + Y;
		case 2: return Y + Z;
		case 3: return -X + Y;
		case 4: return -X + Z;
		case 5: return -X - Y;
		case 6: return -Y + Z;
		case 7: return X - Y;
		case 8: return X - Z;
		case 9: return Y - Z;
		case 10: return -X - Z;
		case 11: return -Y - Z;
			// 4 vertices of regular tetrahedron
		case 12: return X + Y;
		case 13: return -X + Y;
		case 14: return -Y + Z;
		case 15: return -Y - Z;
			// can't happen
		default: return 0;
		}
	}

	// Curve w/ second derivative vanishing at 0 and 1, from Perlin's improved noise paper
	FORCEINLINE float SmoothCurve(float X)
	{
		return X * X * X * (X * (X * 6.0f - 15.0f) + 10.0f);
	}
};

FNoiseMathUtility::FNoiseMathUtility(
		const int InSeed = 1337,
		const float InFrequency = 0.01f,
		const bool InIsAnalog = false,
		const float InThreshold = 0.0f,
		const int InOctaves = 1,
		const float InLacunarity = 2.0f,
		const float InGain = 0.5f,
		const float InWeightedStrength = 0.0f
	)
{
	SetSeed(InSeed);
	
	Frequency = InFrequency;
	
	bIsAnalog = InIsAnalog;
	Threshold = InThreshold;
	
	Octaves = InOctaves;
	Lacunarity = InLacunarity;
	Gain = InGain;
	WeightedStrength = InWeightedStrength;
	CalculateFractalBounding();
}

FNoiseMathUtility::FNoiseMathUtility(const FNoiseParameter& InParam)
{
	SetSeed(InParam.Seed);
	
	Frequency = InParam.Frequency;

	bIsAnalog = InParam.bIsAnalog;
	Threshold = InParam.Threshold;
	
	Octaves = InParam.Octaves;
	Lacunarity = InParam.Lacunarity;
	Gain = InParam.Gain;
	WeightedStrength = InParam.WeightedStrength;
	CalculateFractalBounding();
}

void FNoiseMathUtility::SetSeed(const int InSeed) {
	using namespace FMathPerlinHelpers;

	if (Seed == InSeed) return;
	
	Seed = InSeed;
	
	FRandomStream RandomStream(InSeed);
	
	TArray<int> Numbers;
	Numbers.SetNumUninitialized(256);
	
	for (int i = 0; i < 256; ++i) {
		Numbers[i] = i;
	}
	
	for (int i = 0; i < 256; ++i) {
		int Index = RandomStream.RandRange(0, 255 - i);
		Permutation[i] = Numbers[Index];
		Permutation[i + 256] = Numbers[Index];
		Numbers.Remove(Index);
	}
};

void FNoiseMathUtility::CalculateFractalBounding()
{
	float AbsGain = FMath::Abs(Gain);
	float Amp = AbsGain;
	float AmpFractal = 1.0f;
	for (int i = 1; i < Octaves; i++)
	{
		AmpFractal += Amp;
		Amp *= AbsGain;
	}
	FractalBounding = 1 / AmpFractal;
}

float FNoiseMathUtility::PerlinNoise2D(const FVector2D& Location)
{
	using namespace FMathPerlinHelpers;

	float Xfl = FMath::FloorToFloat((float)Location.X);		// LWC_TODO: Precision loss
	float Yfl = FMath::FloorToFloat((float)Location.Y);
	int32 Xi = (int32)(Xfl) & 255;
	int32 Yi = (int32)(Yfl) & 255;
	float X = (float)Location.X - Xfl;
	float Y = (float)Location.Y - Yfl;
	float Xm1 = X - 1.0f;
	float Ym1 = Y - 1.0f;

	const int32 *P = Permutation;
	int32 AA = P[Xi] + Yi;
	int32 AB = AA + 1;
	int32 BA = P[Xi + 1] + Yi;
	int32 BB = BA + 1;

	float U = SmoothCurve(X);
	float V = SmoothCurve(Y);

	// Note: Due to the choice of Grad2, this will be in the (-1,1) range with no additional scaling
	return FMath::Lerp(
			FMath::Lerp(Grad2(P[AA], X, Y), Grad2(P[BA], Xm1, Y), U),
			FMath::Lerp(Grad2(P[AB], X, Ym1), Grad2(P[BB], Xm1, Ym1), U),
			V);
}

float FNoiseMathUtility::ModifiedPerlinNoise2D(const FVector2D& Location)
{
	double X = Location.X;
	double Y = Location.Y;
	
	float Sum = 0;
	float Amp = FractalBounding;
	
	for (int i = 0; i < Octaves; i++)
	{
		float Noise = FNoiseMathUtility::PerlinNoise2D(FVector2D(X * Frequency, Y * Frequency));
		Sum += Noise * Amp;
		Amp *= FMath::Lerp(1.0f, (Noise + 1.0f) * 0.5f, WeightedStrength);
		Amp *= Gain;

		X *= Lacunarity;
		Y *= Lacunarity;
	}

	if (!bIsAnalog)
	{
		return Sum < Threshold ? -1.0f : 1.0f;
	}

	return Sum;
}

float FNoiseMathUtility::PerlinNoise3D(const FVector& Location)
{
	using namespace FMathPerlinHelpers;

	float Xfl = FMath::FloorToFloat((float)Location.X);		// LWC_TODO: Precision loss
	float Yfl = FMath::FloorToFloat((float)Location.Y);
	float Zfl = FMath::FloorToFloat((float)Location.Z);
	int32 Xi = (int32)(Xfl) & 255;
	int32 Yi = (int32)(Yfl) & 255;
	int32 Zi = (int32)(Zfl) & 255;
	float X = (float)Location.X - Xfl;
	float Y = (float)Location.Y - Yfl;
	float Z = (float)Location.Z - Zfl;
	float Xm1 = X - 1.0f;
	float Ym1 = Y - 1.0f;
	float Zm1 = Z - 1.0f;

	const int32 *P = Permutation;
	int32 A = P[Xi] + Yi;
	int32 AA = P[A] + Zi;	int32 AB = P[A + 1] + Zi;

	int32 B = P[Xi + 1] + Yi;
	int32 BA = P[B] + Zi;	int32 BB = P[B + 1] + Zi;

	float U = SmoothCurve(X);
	float V = SmoothCurve(Y);
	float W = SmoothCurve(Z);
	
	// Note: range is already approximately -1,1 because of the specific choice of direction vectors for the Grad3 function
	// This analysis (http://digitalfreepen.com/2017/06/20/range-perlin-noise.html) suggests scaling by 1/sqrt(3/4) * 1/maxGradientVectorLen, but the choice of gradient vectors makes this overly conservative
	// Scale factor of .97 is (1.0/the max values of a billion random samples); to be 100% sure about the range I also just Clamp it for now.
	return FMath::Clamp(0.97f *
		FMath::Lerp(FMath::Lerp(FMath::Lerp(Grad3(P[AA], X, Y, Z), Grad3(P[BA], Xm1, Y, Z), U),
				 FMath::Lerp(Grad3(P[AB], X, Ym1, Z), Grad3(P[BB], Xm1, Ym1, Z), U),
				 V),
			 FMath::Lerp(FMath::Lerp(Grad3(P[AA + 1], X, Y, Zm1), Grad3(P[BA + 1], Xm1, Y, Zm1), U),
				 FMath::Lerp(Grad3(P[AB + 1], X, Ym1, Zm1), Grad3(P[BB + 1], Xm1, Ym1, Zm1), U),
				 V),
			 W
		),
		-1.0f, 1.0f);
}

float FNoiseMathUtility::ModifiedPerlinNoise3D(const FVector& Location)
{
	double X = Location.X;
	double Y = Location.Y;
	double Z = Location.Z;
	
	float Sum = 0;
	float Amp = FractalBounding;
	
	for (int i = 0; i < Octaves; i++)
	{
		float Noise = FNoiseMathUtility::PerlinNoise3D(FVector(X * Frequency, Y * Frequency, Z * Frequency));
		Sum += Noise * Amp;
		Amp *= FMath::Lerp(1.0f, (Noise + 1.0f) * 0.5f, WeightedStrength);
		Amp *= Gain;

		X *= Lacunarity;
		Y *= Lacunarity;
		Z *= Lacunarity;
	}

	if (!bIsAnalog)
	{
		return Sum < Threshold ? -1.0f : 1.0f;
	}

	return Sum;
}