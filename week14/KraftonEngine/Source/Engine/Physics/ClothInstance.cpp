#include "Physics/ClothInstance.h"

#include "Mesh/Static/StaticMeshAsset.h"
#include "Physics/NvClothSDK.h"
#include "Physics/PhysXConversions.h"

#include <cfloat>

bool FClothInstance::InitializeGrid(const FClothDesc& Desc)
{
	Release();

	Particles.clear();
	PhaseIndices.clear();
	Sets.clear();
	RestValues.clear();
	StiffnessValues.clear();
	Indices.clear();
	Anchors.clear();
	TetherLengths.clear();
	Triangles.clear();

	GridWidth = Desc.Width;
	GridHeight = Desc.Height;
	RenderVertices.clear();
	RenderRevision = 0;

	SourceUVs.clear();
	SourceTangents.clear();
	bUseSourceMeshAttributes = false;

	auto IsPinned = [&](int32 x, int32 y)
	{
		switch (Desc.PinMode)
		{
		case EClothPinMode::TopRow:
			return y == 0;

		case EClothPinMode::TopCorners:
			return y == 0 && (x == 0 || x == Desc.Width - 1);

		case EClothPinMode::LeftEdge:
			return x == 0;

		case EClothPinMode::None:
		default:
			return false;
		}
	};

	for (int32 y = 0; y < Desc.Height; ++y)
	{
		for (int32 x = 0; x < Desc.Width; ++x)
		{
			const float InvMass = IsPinned(x, y) ? 0.0f : 1.0f;
			Particles.push_back(physx::PxVec4(x * Desc.Spacing + Desc.InitialOffset.X,
				y * Desc.Spacing + Desc.InitialOffset.Y,
				Desc.InitialOffset.Z,
				InvMass));
		}
	}

	if (Desc.bUseTether && Desc.PinMode != EClothPinMode::None)
	{
		for (int32 y = 0; y < Desc.Height; ++y)
		{
			for (int32 x = 0; x < Desc.Width; ++x)
			{
				const uint32 ParticleIndex = y * Desc.Width + x;
				const uint32 AnchorIndex = x; // top row, same column

				const physx::PxVec4& P = Particles[ParticleIndex];
				const physx::PxVec4& A = Particles[AnchorIndex];

				const float Dx = P.x - A.x;
				const float Dy = P.y - A.y;
				const float Dz = P.z - A.z;

				Anchors.push_back(AnchorIndex);
				TetherLengths.push_back(std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz));
			}
		}
	}

	constexpr int32 StructuralSetStart = 0;
	constexpr int32 ShearSetStart = 4;
	constexpr int32 BendingSetStart = 8;
	constexpr int32 ConstraintSetCount = 12;

	TArray<uint32> SetIndices[ConstraintSetCount];
	TArray<float> SetRestValues[ConstraintSetCount];

	auto AddConstraintToSet = [&](int32 SetIndex, uint32 A, uint32 B)
	{
		SetIndices[SetIndex].push_back(A);
		SetIndices[SetIndex].push_back(B);

		const physx::PxVec4& PA = Particles[A];
		const physx::PxVec4& PB = Particles[B];

		const float Dx = PA.x - PB.x;
		const float Dy = PA.y - PB.y;
		const float Dz = PA.z - PB.z;
		SetRestValues[SetIndex].push_back(std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz));
	};

	for (int32 y = 0; y < Desc.Height; ++y)
	{
		for (int32 x = 0; x + 1 < Desc.Width; ++x)
		{
			const uint32 A = y * Desc.Width + x;
			const uint32 B = A + 1;

			const int32 SetIndex = (x % 2 == 0) ? 0 : 1;
			AddConstraintToSet(SetIndex, A, B);
		}
	}

	for (int32 y = 0; y + 1 < Desc.Height; ++y)
	{
		for (int32 x = 0; x < Desc.Width; ++x)
		{
			const uint32 A = y * Desc.Width + x;
			const uint32 B = A + Desc.Width;

			const int32 SetIndex = (y % 2 == 0) ? 2 : 3;
			AddConstraintToSet(SetIndex, A, B);
		}
	}

	if (Desc.bUseShear)
	{
		for (int32 y = 0; y + 1 < Desc.Height; ++y)
		{
			for (int32 x = 0; x + 1 < Desc.Width; ++x)
			{
				const uint32 I0 = y * Desc.Width + x;
				const uint32 I1 = I0 + 1;
				const uint32 I2 = I0 + Desc.Width;
				const uint32 I3 = I2 + 1;

				AddConstraintToSet(4 + ((x + y) % 2), I0, I3);
				AddConstraintToSet(6 + ((x + y) % 2), I1, I2);
			}
		}
	}

	if (Desc.bUseBending)
	{
		for (int32 y = 0; y < Desc.Height; ++y)
		{
			for (int32 x = 0; x + 2 < Desc.Width; ++x)
			{
				const uint32 A = y * Desc.Width + x;
				const uint32 B = A + 2;
				AddConstraintToSet(8 + (x % 2), A, B);
			}
		}

		for (int32 y = 0; y + 2 < Desc.Height; ++y)
		{
			for (int32 x = 0; x < Desc.Width; ++x)
			{
				const uint32 A = y * Desc.Width + x;
				const uint32 B = A + 2 * Desc.Width;
				AddConstraintToSet(10 + (y % 2), A, B);
			}
		}
	}

	for (int32 SetIndex = 0; SetIndex < ConstraintSetCount; ++SetIndex)
	{
		Indices.insert(Indices.end(), SetIndices[SetIndex].begin(), SetIndices[SetIndex].end());
		RestValues.insert(RestValues.end(), SetRestValues[SetIndex].begin(), SetRestValues[SetIndex].end());

		Sets.push_back(static_cast<uint32>(RestValues.size()));
		PhaseIndices.push_back(static_cast<uint32>(SetIndex));
	}

	for (int32 y = 0; y + 1 < Desc.Height; ++y)
	{
		for (int32 x = 0; x + 1 < Desc.Width; ++x)
		{
			uint32 I0 = y * Desc.Width + x;
			uint32 I1 = I0 + 1;
			uint32 I2 = I0 + Desc.Width;
			uint32 I3 = I2 + 1;

			Triangles.push_back(I0);
			Triangles.push_back(I1);
			Triangles.push_back(I2);

			Triangles.push_back(I1);
			Triangles.push_back(I3);
			Triangles.push_back(I2);
		}
	}

	auto MakeRange = [](auto& Array)
	{
		using T = typename std::remove_reference_t<decltype(Array)>::value_type;
		return nv::cloth::Range<T>(Array.data(), Array.data() + Array.size());
	};

	nv::cloth::Factory* Factory = FNvClothSDK::Get().GetFactory();

	Fabric = Factory->createFabric(static_cast<uint32>(Particles.size()),
		MakeRange(PhaseIndices), MakeRange(Sets), MakeRange(RestValues), MakeRange(StiffnessValues),
		MakeRange(Indices),MakeRange(Anchors), MakeRange(TetherLengths), MakeRange(Triangles));

	Cloth = Factory->createCloth(MakeRange(Particles), *Fabric);
	ApplySimulationSettings(Desc);
	UpdateCollision(FClothCollisionData());

	UE_LOG("Cloth PinMode=%d UseTether=%d Gravity=(%.2f %.2f %.2f)",
		static_cast<int32>(Desc.PinMode),
		Desc.bUseTether ? 1 : 0,
		Desc.Gravity.X, Desc.Gravity.Y, Desc.Gravity.Z);

	FNvClothSDK::Get().GetSolver()->addCloth(Cloth);

	UpdateRenderVerticesFromParticles(Desc.RenderNormalOffset);

	return true;
}

bool FClothInstance::InitializeMesh(const FClothDesc& Desc, const FMeshDataView& MeshView)
{
	Release();

	Particles.clear();
	PhaseIndices.clear();
	Sets.clear();
	RestValues.clear();
	StiffnessValues.clear();
	Indices.clear();
	Anchors.clear();
	TetherLengths.clear();
	Triangles.clear();
	RenderVertices.clear();
	SourceUVs.clear();
	SourceTangents.clear();

	RenderRevision = 0;
	GridWidth = 0;
	GridHeight = 0;
	bUseSourceMeshAttributes = true;

	if (!MeshView.IsValid() || MeshView.VertexCount == 0 || MeshView.IndexCount < 3) return false;

	SourceUVs.resize(MeshView.VertexCount, FVector2(0.0f, 0.0f));
	SourceTangents.resize(MeshView.VertexCount, FVector4(1.0f, 0.0f, 0.0f, 1.0f));

	float MaxZ = -FLT_MAX;
	for (uint32 i = 0; i < MeshView.VertexCount; ++i)
	{
		const FVector& P = MeshView.GetPosition(i);
		MaxZ = std::max(MaxZ, P.Z);
	}

	const float PinThreshold = Desc.Spacing;

	for (uint32 i = 0; i < MeshView.VertexCount; ++i)
	{
		const FNormalVertex& V = MeshView.GetVertex<FNormalVertex>(i);
		const bool bPinned = Desc.PinMode != EClothPinMode::None && V.pos.Z >= MaxZ - PinThreshold;

		const float InvMass = bPinned ? 0.0f : 1.0f;

		Particles.push_back(physx::PxVec4(V.pos.X + Desc.InitialOffset.X,
			V.pos.Y + Desc.InitialOffset.Y,
			V.pos.Z + Desc.InitialOffset.Z,
			InvMass));

		SourceUVs[i] = V.tex;
		SourceTangents[i] = V.tangent;
	}

	Triangles.reserve(MeshView.IndexCount);
	for (uint32 i = 0; i + 2 < MeshView.IndexCount; i += 3)
	{
		const uint32 I0 = MeshView.IndexData[i + 0];
		const uint32 I1 = MeshView.IndexData[i + 1];
		const uint32 I2 = MeshView.IndexData[i + 2];

		if (I0 < MeshView.VertexCount && I1 < MeshView.VertexCount && I2 < MeshView.VertexCount &&
			I0 != I1 && I1 != I2 && I2 != I0)
		{
			Triangles.push_back(I0);
			Triangles.push_back(I1);
			Triangles.push_back(I2);
		}
	}

	constexpr int32 MeshConstraintSetCount = 8;
	TArray<uint32> SetIndices[MeshConstraintSetCount];
	TArray<float> SetRestValues[MeshConstraintSetCount];
	TSet<uint32> UsedParticlesPerSet[MeshConstraintSetCount];

	auto AddConstraintToSet = [&](int32 SetIndex, uint32 A, uint32 B)
	{
		if (A == B || A >= Particles.size() || B >= Particles.size()) return;

		SetIndices[SetIndex].push_back(A);
		SetIndices[SetIndex].push_back(B);

		const physx::PxVec4& PA = Particles[A];
		const physx::PxVec4& PB = Particles[B];

		const float Dx = PA.x - PB.x;
		const float Dy = PA.y - PB.y;
		const float Dz = PA.z - PB.z;
		SetRestValues[SetIndex].push_back(std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz));
	};

	auto AddConstraintToBestSet = [&](uint32 A, uint32 B)
	{
		if (A == B || A >= Particles.size() || B >= Particles.size()) return;

		int32 BestSet = -1;
		for (int32 SetIndex = 0; SetIndex < MeshConstraintSetCount; ++SetIndex)
		{
			const bool bUsesA = UsedParticlesPerSet[SetIndex].find(A) != UsedParticlesPerSet[SetIndex].end();
			const bool bUsesB = UsedParticlesPerSet[SetIndex].find(B) != UsedParticlesPerSet[SetIndex].end();
			if (!bUsesA && !bUsesB)
			{
				BestSet = SetIndex;
				break;
			}
		}

		if (BestSet < 0)
		{
			BestSet = 0;
			for (int32 SetIndex = 1; SetIndex < MeshConstraintSetCount; ++SetIndex)
			{
				if (SetIndices[SetIndex].size() < SetIndices[BestSet].size())
				{
					BestSet = SetIndex;
				}
			}
		}

		AddConstraintToSet(BestSet, A, B);
		UsedParticlesPerSet[BestSet].insert(A);
		UsedParticlesPerSet[BestSet].insert(B);
	};

	TSet<uint64> UniqueEdges;

	auto AddEdge = [&](uint32 A, uint32 B)
	{
		if (A > B) std::swap(A, B);

		const uint64 Key = (static_cast<uint64>(A) << 32) | B;
		if (UniqueEdges.insert(Key).second)
		{
			AddConstraintToBestSet(A, B);
		}
	};
	for (uint32 i = 0; i + 2 < Triangles.size(); i += 3)
	{
		const uint32 I0 = Triangles[i + 0];
		const uint32 I1 = Triangles[i + 1];
		const uint32 I2 = Triangles[i + 2];

		AddEdge(I0, I1);
		AddEdge(I1, I2);
		AddEdge(I2, I0);
	}

	for (int32 SetIndex = 0; SetIndex < MeshConstraintSetCount; ++SetIndex)
	{
		Indices.insert(Indices.end(), SetIndices[SetIndex].begin(), SetIndices[SetIndex].end());
		RestValues.insert(RestValues.end(), SetRestValues[SetIndex].begin(), SetRestValues[SetIndex].end());

		Sets.push_back(static_cast<uint32>(RestValues.size()));
		PhaseIndices.push_back(static_cast<uint32>(SetIndex));
	}

	if (RestValues.empty())
	{
		return false;
	}

	auto MakeRange = [](auto& Array)
	{
		using T = typename std::remove_reference_t<decltype(Array)>::value_type;
		return nv::cloth::Range<T>(Array.data(), Array.data() + Array.size());
	};

	nv::cloth::Factory* Factory = FNvClothSDK::Get().GetFactory();
	if (!Factory) return false;

	Fabric = Factory->createFabric(
		static_cast<uint32>(Particles.size()),
		MakeRange(PhaseIndices),
		MakeRange(Sets),
		MakeRange(RestValues),
		MakeRange(StiffnessValues),
		MakeRange(Indices),
		MakeRange(Anchors),
		MakeRange(TetherLengths),
		MakeRange(Triangles));

	if (!Fabric) return false;

	Cloth = Factory->createCloth(MakeRange(Particles), *Fabric);
	if (!Cloth) return false;

	ApplySimulationSettings(Desc);
	UpdateCollision(FClothCollisionData());

	if (nv::cloth::Solver* Solver = FNvClothSDK::Get().GetSolver())
	{
		Solver->addCloth(Cloth);
	}

	UpdateRenderVerticesFromParticles(Desc.RenderNormalOffset);

	return true;
}

void FClothInstance::Release()
{
	nv::cloth::Solver* Solver = FNvClothSDK::Get().GetSolver();

	if (Solver && Cloth)
	{
		Solver->removeCloth(Cloth);
	}

	if (Cloth)
	{
		delete Cloth;
		Cloth = nullptr;
	}

	if (Fabric)
	{
		Fabric->decRefCount();
		Fabric = nullptr;
	}
}

void FClothInstance::UpdateRenderVerticesFromParticles(float RenderNormalOffset)
{
	if (!Cloth) return;

	auto Current = Cloth->getCurrentParticles();

	RenderVertices.resize(Current.size());

	for (uint32 i = 0; i < Current.size(); ++i)
	{
		FVertexPNCTT& V = RenderVertices[i];
		const physx::PxVec4& P = Current[i];

		V.Position = FVector(P.x, P.y, P.z);
		V.Color = FVector4(1, 1, 1, 1);

		if (bUseSourceMeshAttributes && i < SourceUVs.size())
		{
			V.UV = SourceUVs[i];
		}
		else
		{
			V.UV = FVector2(
				GridWidth > 1 ? float(i % GridWidth) / float(GridWidth - 1) : 0.0f,
				GridHeight > 1 ? float(i / GridWidth) / float(GridHeight - 1) : 0.0f);
		}
	}

	TArray<FVector> NormalSums;
	NormalSums.resize(RenderVertices.size(), FVector(0, 0, 0));

	for (int32 i = 0; i + 2 < Triangles.size(); i += 3)
	{
		uint32 I0 = Triangles[i + 0];
		uint32 I1 = Triangles[i + 1];
		uint32 I2 = Triangles[i + 2];

		const FVector& P0 = RenderVertices[I0].Position;
		const FVector& P1 = RenderVertices[I1].Position;
		const FVector& P2 = RenderVertices[I2].Position;

		FVector FaceNormal = (P1 - P0).Cross(P2 - P0);
		const float Area2 = FaceNormal.Length();

		if (Area2 > 1.0e-6f)
		{
			// Normalize하지 않은 cross vector를 그대로 더하면 면적 가중 normal이 됨
			NormalSums[I0] += FaceNormal;
			NormalSums[I1] += FaceNormal;
			NormalSums[I2] += FaceNormal;
		}
	}

	for (int32 i = 0; i < RenderVertices.size(); ++i)
	{
		if (NormalSums[i].LengthSquared() > 1.0e-8f)
		{
			NormalSums[i].Normalize();
			RenderVertices[i].Normal = NormalSums[i];
		}
		else
		{
			RenderVertices[i].Normal = FVector(0, 0, 1);
		}
	}

	if (bUseSourceMeshAttributes)
	{
		for (int32 i = 0; i < RenderVertices.size(); ++i)
		{
			RenderVertices[i].Tangent = i < SourceTangents.size()
				? SourceTangents[i]
				: FVector4(1.0f, 0.0f, 0.0f, 1.0f);
		}
	}
	else
	{
		auto GetIndex = [this](int32 X, int32 Y)
		{
			return Y * GridWidth + X;
		};

		for (int32 y = 0; y < GridHeight; ++y)
		{
			for (int32 x = 0; x < GridWidth; ++x)
			{
				const int32 i = GetIndex(x, y);

				const int32 x0 = std::max(0, x - 1);
				const int32 x1 = std::min(GridWidth - 1, x + 1);

				FVector Tangent = RenderVertices[GetIndex(x1, y)].Position -
					RenderVertices[GetIndex(x0, y)].Position;

				if (Tangent.LengthSquared() > 1.0e-8f)
				{
					Tangent.Normalize();
				}
				else
				{
					Tangent = FVector(1, 0, 0);
				}

				RenderVertices[i].Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, 1.0f);
			}
		}
	}

	if (RenderNormalOffset > 0.0f)
	{
		for (FVertexPNCTT& Vertex : RenderVertices)
		{
			Vertex.Position += Vertex.Normal * RenderNormalOffset;
		}
	}

	++RenderRevision;
}

void FClothInstance::Simulate(float DeltaTime, int32 SubstepCount, float RenderNormalOffset)
{
	nv::cloth::Solver* Solver = FNvClothSDK::Get().GetSolver();
	if (!Solver || !Cloth) return;

	DeltaTime = std::min(DeltaTime, 1.0f / 60.0f);
	SubstepCount = std::max(1, SubstepCount);

	const float StepDeltaTime = DeltaTime / static_cast<float>(SubstepCount);

	for (int32 Step = 0; Step < SubstepCount; ++Step)
	{
		if (Solver->beginSimulation(StepDeltaTime))
		{
			const int32 ChunkCount = Solver->getSimulationChunkCount();
			for (int32 i = 0; i < ChunkCount; ++i)
			{
				Solver->simulateChunk(i);
			}
			Solver->endSimulation();
		}
	}

	UpdateRenderVerticesFromParticles(RenderNormalOffset);
}

void FClothInstance::ApplySimulationSettings(const FClothDesc& Desc)
{
	if (!Cloth) return;

	Cloth->setGravity(physx::PxVec3(Desc.Gravity.X, Desc.Gravity.Y, Desc.Gravity.Z));
	Cloth->setSolverFrequency(Desc.SolverFrequency);
	Cloth->setStiffnessFrequency(Desc.StiffnessFrequency);

	Cloth->setCollisionMassScale(Desc.CollisionMassScale);
	Cloth->enableContinuousCollision(Desc.bEnableCCD);
	Cloth->setFriction(Desc.Friction);

	Cloth->setDamping(physx::PxVec3(Desc.Damping.X, Desc.Damping.Y, Desc.Damping.Z));
	Cloth->setLinearDrag(physx::PxVec3(Desc.LinearDrag.X, Desc.LinearDrag.Y, Desc.LinearDrag.Z));
	Cloth->setAngularDrag(physx::PxVec3(Desc.AngularDrag.X, Desc.AngularDrag.Y, Desc.AngularDrag.Z));

	TArray<nv::cloth::PhaseConfig> PhaseConfigs;
	PhaseConfigs.reserve(PhaseIndices.size());

	for (uint32 PhaseIndex : PhaseIndices)
	{
		nv::cloth::PhaseConfig Config(static_cast<uint16>(PhaseIndex));

		if (PhaseIndex < 4)
			Config.mStiffness = Desc.StructuralStiffness;
		else if (PhaseIndex < 8)
			Config.mStiffness = Desc.ShearStiffness;
		else
			Config.mStiffness = Desc.BendingStiffness;

		Config.mStiffnessMultiplier = 1.0f;
		Config.mCompressionLimit = 1.0f;
		Config.mStretchLimit = 1.0f;

		PhaseConfigs.push_back(Config);
	}

	Cloth->setPhaseConfig(
		nv::cloth::Range<const nv::cloth::PhaseConfig>(
		PhaseConfigs.data(),
		PhaseConfigs.data() + PhaseConfigs.size()));
}

void FClothInstance::UpdateCollision(const FClothCollisionData& Data)
{
	if (!Cloth) return;

	// capsules는 spheres를 참조하므로 먼저 제거
	Cloth->setCapsules(
		nv::cloth::Range<const uint32>(),
		0,
		Cloth->getNumCapsules());

	// convexes는 planes를 참조하므로 먼저 제거
	Cloth->setConvexes(
		nv::cloth::Range<const uint32>(),
		0,
		Cloth->getNumConvexes());

	Cloth->setSpheres(
		nv::cloth::Range<const physx::PxVec4>(
		Data.Spheres.data(),
		Data.Spheres.data() + Data.Spheres.size()),
		0,
		Cloth->getNumSpheres());

	Cloth->setPlanes(
		nv::cloth::Range<const physx::PxVec4>(
		Data.Planes.data(),
		Data.Planes.data() + Data.Planes.size()),
		0,
		Cloth->getNumPlanes());

	Cloth->setCapsules(
		nv::cloth::Range<const uint32>(
		Data.Capsules.data(),
		Data.Capsules.data() + Data.Capsules.size()),
		0,
		0);

	Cloth->setConvexes(
		nv::cloth::Range<const uint32>(
		Data.ConvexMasks.data(),
		Data.ConvexMasks.data() + Data.ConvexMasks.size()),
		0,
		0);
}

void FClothInstance::SetSimulationSpaceTransform(const FVector& WorldLocation, const FQuat& WorldRotation, bool bTeleport)
{
	if (!Cloth) return;

	const physx::PxVec3 PxLocation = ToPxVec3(WorldLocation);
	const physx::PxQuat PxRotation = ToPxQuat(WorldRotation);

	if (bTeleport)
	{
		Cloth->teleportToLocation(PxLocation, PxRotation);
		Cloth->clearInertia();
		return;
	}

	Cloth->setTranslation(PxLocation);
	Cloth->setRotation(PxRotation);
}
