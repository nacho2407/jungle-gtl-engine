#include "Particles/Beam/ParticleModuleBeamModifier.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleBeamModifier::UParticleModuleBeamModifier()
	: bAbsoluteTangent(false)
{
	InitializeDefaults();
}

void UParticleModuleBeamModifier::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

uint32 UParticleModuleBeamModifier::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(FBeamParticleModifierPayloadData);
}

void UParticleModuleBeamModifier::Spawn(const FSpawnContext& Context)
{
	int32 CurrentOffset = Context.Offset;
	FBeam2TypeDataPayload* BeamData = nullptr;
	FBeamParticleModifierPayloadData* SourceModifier = nullptr;
	FBeamParticleModifierPayloadData* TargetModifier = nullptr;
	GetDataPointers(&Context.Owner, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, BeamData, SourceModifier, TargetModifier);
	FBeamParticleModifierPayloadData* Payload = ModifierType == PEB2MT_Source ? SourceModifier : TargetModifier;
	if (!Payload) return;

	Payload->bModifyPosition = PositionOptions.bModify;
	Payload->bScalePosition = PositionOptions.bScale;
	Payload->Position = Position.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
	Payload->bModifyTangent = TangentOptions.bModify;
	Payload->bScaleTangent = TangentOptions.bScale;
	Payload->Tangent = Tangent.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
	Payload->bModifyStrength = StrengthOptions.bModify;
	Payload->bScaleStrength = StrengthOptions.bScale;
	Payload->Strength = Strength.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
}

void UParticleModuleBeamModifier::Update(const FUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP;
	int32 LocalOffset = Context.Offset;
	FBeam2TypeDataPayload* BeamData = nullptr;
	FBeamParticleModifierPayloadData* SourceModifier = nullptr;
	FBeamParticleModifierPayloadData* TargetModifier = nullptr;
	GetDataPointers(&Context.Owner, ParticleBase, LocalOffset, BeamData, SourceModifier, TargetModifier);
	FBeamParticleModifierPayloadData* Payload = ModifierType == PEB2MT_Source ? SourceModifier : TargetModifier;
	if (Payload)
	{
		Payload->bModifyPosition = PositionOptions.bModify;
		Payload->bScalePosition = PositionOptions.bScale;
		Payload->Position = Position.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		Payload->bModifyTangent = TangentOptions.bModify;
		Payload->bScaleTangent = TangentOptions.bScale;
		Payload->Tangent = Tangent.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		Payload->bModifyStrength = StrengthOptions.bModify;
		Payload->bScaleStrength = StrengthOptions.bScale;
		Payload->Strength = Strength.GetValue(Particle.RelativeTime, Context.GetDistributionData());
	}
	END_UPDATE_LOOP;
}

void UParticleModuleBeamModifier::GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, FBeam2TypeDataPayload*& BeamDataPayload,
	FBeamParticleModifierPayloadData*& SourceModifierPayload,
	FBeamParticleModifierPayloadData*& TargetModifierPayload)
{
	BeamDataPayload = reinterpret_cast<FBeam2TypeDataPayload*>(const_cast<uint8*>(ParticleBase) + Owner->TypeDataOffset);
	SourceModifierPayload = ModifierType == PEB2MT_Source
		? reinterpret_cast<FBeamParticleModifierPayloadData*>(const_cast<uint8*>(ParticleBase) + CurrentOffset)
		: nullptr;
	TargetModifierPayload = ModifierType == PEB2MT_Target
		? reinterpret_cast<FBeamParticleModifierPayloadData*>(const_cast<uint8*>(ParticleBase) + CurrentOffset)
		: nullptr;
	CurrentOffset += sizeof(FBeamParticleModifierPayloadData);
}

void UParticleModuleBeamModifier::GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, int32& BeamDataOffset, int32& SourceModifierOffset,
	int32& TargetModifierOffset)
{
	BeamDataOffset = Owner ? Owner->TypeDataOffset : INDEX_NONE;
	SourceModifierOffset = ModifierType == PEB2MT_Source ? CurrentOffset : INDEX_NONE;
	TargetModifierOffset = ModifierType == PEB2MT_Target ? CurrentOffset : INDEX_NONE;
	CurrentOffset += sizeof(FBeamParticleModifierPayloadData);
}

void UParticleModuleBeamModifier::Serialize(FArchive& Ar)
{
	UParticleModuleBeamBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(ModifierType);
	bool PositionModify = PositionOptions.bModify;
	bool PositionScale = PositionOptions.bScale;
	bool PositionLock = PositionOptions.bLock;
	Ar << PositionModify << PositionScale << PositionLock;
	Position.Serialize(Ar);
	bool TangentModify = TangentOptions.bModify;
	bool TangentScale = TangentOptions.bScale;
	bool TangentLock = TangentOptions.bLock;
	Ar << TangentModify << TangentScale << TangentLock;
	Tangent.Serialize(Ar);
	bool AbsoluteTangent = bAbsoluteTangent;
	Ar << AbsoluteTangent;
	bool StrengthModify = StrengthOptions.bModify;
	bool StrengthScale = StrengthOptions.bScale;
	bool StrengthLock = StrengthOptions.bLock;
	Ar << StrengthModify << StrengthScale << StrengthLock;
	Strength.Serialize(Ar);
	if (Ar.IsLoading())
	{
		PositionOptions.bModify = PositionModify;
		PositionOptions.bScale = PositionScale;
		PositionOptions.bLock = PositionLock;
		TangentOptions.bModify = TangentModify;
		TangentOptions.bScale = TangentScale;
		TangentOptions.bLock = TangentLock;
		bAbsoluteTangent = AbsoluteTangent;
		StrengthOptions.bModify = StrengthModify;
		StrengthOptions.bScale = StrengthScale;
		StrengthOptions.bLock = StrengthLock;
	}
}
