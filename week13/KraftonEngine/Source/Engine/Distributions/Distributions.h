#pragma once
#include "Core/Types/CoreTypes.h"
#include "Math/MathUtils.h"

class FArchive;

/**
 * Operation to perform when looking up a value.
 */
enum ERawDistributionOperation
{
	RDO_Uninitialized,
	RDO_None,
};

/**
 * Lookup table used to sample distributions at runtime.
 */
struct FDistributionLookupTable
{
	/** Time between values in the lookup table */
	float TimeScale;
	/** Absolute time of the first value */
	float TimeBias;
	/** Values in the table. */
	TArray<float> Values;
	/** Operation for which the table was built. */
	uint8 Op;
	/** Number of entries in the table. */
	uint8 EntryCount;
	/** Number of values between entries [1,8]. */
	uint8 EntryStride;
	/** Number of values between sub-entries [0,4]. */
	uint8 SubEntryStride;
	/** Lock axes flag for vector distributions. */
	uint8 LockFlag;

	/** Default constructor. */
	FDistributionLookupTable()
		: TimeScale(0.0f)
		, TimeBias(0.0f)
		, Op(RDO_Uninitialized)
		, EntryCount(0)
		, EntryStride(0)
		, SubEntryStride(0)
		, LockFlag(0)
	{}

	/**
	 * Empties the table of all values.
	 */
	void Empty()
	{
		Op = RDO_Uninitialized;
		EntryCount = 0;
		EntryStride = 0;
		SubEntryStride = 0;
		TimeScale = 0.0f;
		TimeBias = 0.0f;
		LockFlag = 0;
	}

	/**
	 * Returns true if the lookup table contains no values.
	 */
	inline bool IsEmpty() const
	{
		return Values.size() == 0 || EntryCount == 0;
	}

	/**
	 * Computes the number of Values per entry in the table.
	 */
	inline float GetValuesPerEntry() const
	{
		return (float)(EntryStride - SubEntryStride);
	}

	/**
	 * Compute the number of values contained in the table.
	 */
	inline float GetValueCount() const
	{
		return (float)(Values.size());
	}

	/**
	 * Get the entry for Time and the one after it for interpolating (along with
	 * an alpha for interpolation)
	 *
	 * @param Time The time we are looking to retrieve
	 * @param Entry1 Out variable that is the first (or only) entry
	 * @param Entry2 Out variable that is the second entry (for interpolating)
	 * @param LerpAlpha Out variable that is the alpha for interpolating between Entry1 and Entry2
	 */
	inline void GetEntry(float Time, const float*& Entry1, const float*& Entry2, float& LerpAlpha) const
	{
		// make time relative to start time
		Time -= TimeBias;
		Time *= TimeScale;
		Time = FMath::FloatSelect(Time, Time, 0.0f);

		// calculate the alpha to lerp between entry1 and entry2
		LerpAlpha = FMath::Fractional(Time);

		// get the entries to lerp between
		const uint32 Index = FMath::TruncToInt(Time);
		const uint32 Index1 = FMath::Min<uint32>(Index + 0, EntryCount - 1) * EntryStride;
		const uint32 Index2 = FMath::Min<uint32>(Index + 1, EntryCount - 1) * EntryStride;
		Entry1 = &Values[Index1];
		Entry2 = &Values[Index2];
	}

	/**
	 * Get the range of values produced by the table.
	 * @note: in the case of a constant curve, this will not be exact!
	 * @param OutMinValues - The smallest values produced by this table.
	 * @param OutMaxValues - The largest values produced by this table.
	 */
	void GetRange(float* OutMinValues, float* OutMaxValues)
	{
		if (EntryCount > 0)
		{
			const int32 ValuesPerEntry = (int32)GetValuesPerEntry();
			const float* Entry = Values.data();

			// Initialize to the first entry in the table.
			for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
			{
				OutMinValues[ValueIndex] = Entry[ValueIndex];
				OutMaxValues[ValueIndex] = Entry[ValueIndex + SubEntryStride];
			}

			// Iterate over each entry updating the minimum and maximum values.
			for (int32 EntryIndex = 1; EntryIndex < EntryCount; ++EntryIndex)
			{
				Entry += EntryStride;
				for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
				{
					OutMinValues[ValueIndex] = FMath::Min(OutMinValues[ValueIndex], Entry[ValueIndex]);
					OutMaxValues[ValueIndex] = FMath::Max(OutMaxValues[ValueIndex], Entry[ValueIndex + SubEntryStride]);
				}
			}
		}
	}
};

#define DIST_GET_RANDOM_VALUE(RandStream)		((RandStream == NULL) ? FMath::SRand() : RandStream->GetFraction())

/**
 * Raw distribution used to quickly sample distributions at runtime.
 */
struct FRawDistribution
{
	FRawDistribution()
	{

	}

	/**
	 * Serialization.
	 * @param Ar - The archive with which to serialize.
	 * @returns true if serialization was successful.
	 */
	bool Serialize(FArchive& Ar);

	/**
	 * Calcuate the float or vector value at the given time
	 * @param Time The time to evaluate
	 * @param Value An array of (1 or 3) FLOATs to receive the values
	 * @param NumCoords The number of floats in the Value array
	 * @param Extreme For distributions that use one of the extremes, this is which extreme to use
	 */
	void GetValue(float Time, float* Value, int32 NumCoords, int32 Extreme, struct FRandomStream* InRandomStream) const;

	// prebaked versions of these
	void GetValue1(float Time, float* Value, int32 Extreme, struct FRandomStream* InRandomStream) const;
	void GetValue3(float Time, float* Value, int32 Extreme, struct FRandomStream* InRandomStream) const;
	inline void GetValue1None(float Time, float* InValue) const
	{
		float* Value = InValue;
		const float* Entry1;
		const float* Entry2;
		float LerpAlpha = 0.0f;
		LookupTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
		const float* NewEntry1 = Entry1;
		const float* NewEntry2 = Entry2;
		Value[0] = FMath::Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
	}

	inline void GetValue3None(float Time, float* InValue) const
	{
		float* Value = InValue;
		const float* Entry1;
		const float* Entry2;
		float LerpAlpha = 0.0f;
		LookupTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
		const float* NewEntry1 = Entry1;
		const float* NewEntry2 = Entry2;
		float T0 = FMath::Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
		float T1 = FMath::Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
		float T2 = FMath::Lerp(NewEntry1[2], NewEntry2[2], LerpAlpha);
		Value[0] = T0;
		Value[1] = T1;
		Value[2] = T2;
	}

	inline bool IsSimple() const
	{
		return LookupTable.Op == RDO_None;
	}

protected:
	/** Lookup table of values */
	FDistributionLookupTable LookupTable;
};
