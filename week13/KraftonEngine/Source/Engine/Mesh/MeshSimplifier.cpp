#include "Mesh/MeshSimplifier.h"

#include <algorithm>
#include <vector>
#include <cmath>

// ============================================================
// Quadric Error Metrics (Garland & Heckbert) 메시 간소화
// — 위치 기반 정점 웰딩으로 UV seam 크랙 방지
// ============================================================

namespace
{
	struct FQuadric
	{
		double D[10] = {};

		static FQuadric FromPlane(double a, double b, double c, double d)
		{
			FQuadric Q;
			Q.D[0]=a*a; Q.D[1]=a*b; Q.D[2]=a*c; Q.D[3]=a*d;
			Q.D[4]=b*b; Q.D[5]=b*c; Q.D[6]=b*d;
			Q.D[7]=c*c; Q.D[8]=c*d;
			Q.D[9]=d*d;
			return Q;
		}

		FQuadric& operator+=(const FQuadric& O) { for (int i=0;i<10;++i) D[i]+=O.D[i]; return *this; }
		FQuadric operator+(const FQuadric& O) const { FQuadric R=*this; R+=O; return R; }

		double Eval(double x, double y, double z) const
		{
			return D[0]*x*x + 2*D[1]*x*y + 2*D[2]*x*z + 2*D[3]*x
				 + D[4]*y*y + 2*D[5]*y*z + 2*D[6]*y
				 + D[7]*z*z + 2*D[8]*z + D[9];
		}

		bool Solve(double& ox, double& oy, double& oz) const
		{
			double a=D[0],b=D[1],c=D[2],e=D[4],f=D[5],i=D[7];
			double det = a*(e*i-f*f) - b*(b*i-f*c) + c*(b*f-e*c);
			if (std::abs(det) < 1e-10) return false;
			double inv=1.0/det;
			double r0=-D[3], r1=-D[6], r2=-D[8];
			ox = inv*((e*i-f*f)*r0 + (c*f-b*i)*r1 + (b*f-c*e)*r2);
			oy = inv*((f*c-b*i)*r0 + (a*i-c*c)*r1 + (b*c-a*f)*r2);
			oz = inv*((b*f-e*c)*r0 + (b*c-a*f)*r1 + (a*e-b*b)*r2);
			return true;
		}
	};

	inline void VecRemove(std::vector<uint32>& V, uint32 Val)
	{
		for (size_t i=0; i<V.size(); ++i)
			if (V[i]==Val) { V[i]=V.back(); V.pop_back(); return; }
	}

	inline bool VecHas(const std::vector<uint32>& V, uint32 Val)
	{
		for (uint32 X : V) if (X==Val) return true;
		return false;
	}

	inline void VecAdd(std::vector<uint32>& V, uint32 Val)
	{
		if (!VecHas(V, Val)) V.push_back(Val);
	}

	struct FCandidate
	{
		float Cost;
		uint32 V0, V1;
		FVector OptPos;
		uint32 Version;
	};

	struct FCandLess { bool operator()(const FCandidate& A, const FCandidate& B) const { return A.Cost > B.Cost; } };

	// ── 위치 기반 정점 웰딩 ──
	// 같은 위치의 중복 정점(UV seam, hard edge)을 병합
	// 반환: WeldMap[원본 인덱스] = 웰딩된 인덱스, WeldedVerts
	void WeldVertices(
		const TArray<FNormalVertex>& InVerts,
		const TArray<uint32>& InIndices,
		TArray<FNormalVertex>& OutVerts,
		TArray<uint32>& OutIndices,
		TArray<uint32>& OutUnweldMap)  // 웰딩된 인덱스 → 원본 첫 인덱스 (속성 복원용)
	{
		const uint32 N = static_cast<uint32>(InVerts.size());

		// 위치 기준 정렬 인덱스
		std::vector<uint32> Order(N);
		for (uint32 i = 0; i < N; ++i) Order[i] = i;

		std::sort(Order.begin(), Order.end(), [&](uint32 A, uint32 B) {
			const FVector& PA = InVerts[A].pos;
			const FVector& PB = InVerts[B].pos;
			if (PA.X != PB.X) return PA.X < PB.X;
			if (PA.Y != PB.Y) return PA.Y < PB.Y;
			return PA.Z < PB.Z;
		});

		// 같은 위치 정점 그룹화
		TArray<uint32> WeldMap(N, UINT32_MAX);
		OutVerts.clear();
		OutUnweldMap.clear();

		const float Eps = 1e-6f;
		for (uint32 i = 0; i < N; )
		{
			uint32 GroupStart = i;
			uint32 Rep = Order[i];
			uint32 NewIdx = static_cast<uint32>(OutVerts.size());

			// 같은 위치의 정점들 — 속성은 첫 번째 것 사용
			FNormalVertex Merged = InVerts[Rep];

			// 그룹 내 모든 노멀 평균
			FVector NormalSum = InVerts[Rep].normal;
			uint32 GroupCount = 1;

			WeldMap[Rep] = NewIdx;
			++i;

			while (i < N)
			{
				uint32 Cur = Order[i];
				const FVector& PC = InVerts[Cur].pos;
				const FVector& PR = InVerts[Rep].pos;
				if (std::abs(PC.X-PR.X) > Eps || std::abs(PC.Y-PR.Y) > Eps || std::abs(PC.Z-PR.Z) > Eps)
					break;
				WeldMap[Cur] = NewIdx;
				NormalSum = NormalSum + InVerts[Cur].normal;
				++GroupCount;
				++i;
			}

			// 노멀 평균 (웰딩 시 hard edge 소프트닝 — LOD에서는 허용)
			if (GroupCount > 1)
			{
				float Len = NormalSum.Length();
				if (Len > 1e-8f) Merged.normal = NormalSum / Len;
			}

			OutVerts.push_back(Merged);
			OutUnweldMap.push_back(Rep);
		}

		// 인덱스 리매핑
		OutIndices.resize(InIndices.size());
		for (size_t i = 0; i < InIndices.size(); ++i)
			OutIndices[i] = WeldMap[InIndices[i]];
	}
}

FSimplifiedMesh FMeshSimplifier::Simplify(
	const TArray<FNormalVertex>& InVertices,
	const TArray<uint32>& InIndices,
	const TArray<FStaticMeshSection>& InSections,
	float TargetRatio)
{
	FSimplifiedMesh Result;

	if (TargetRatio >= 1.0f || InIndices.size() < 3 || InVertices.empty())
	{
		Result.Vertices = InVertices; Result.Indices = InIndices; Result.Sections = InSections;
		return Result;
	}

	// ── 1단계: 위치 기반 정점 웰딩 (UV seam 크랙 방지) ──
	TArray<FNormalVertex> WV;
	TArray<uint32> WI;
	TArray<uint32> UnweldMap;
	WeldVertices(InVertices, InIndices, WV, WI, UnweldMap);

	// 웰딩 후 degenerate 삼각형 제거
	TArray<uint32> CleanI;
	CleanI.reserve(WI.size());
	const uint32 RawTris = static_cast<uint32>(WI.size()) / 3;
	TArray<uint32> OldToCleanTri(RawTris, UINT32_MAX);
	uint32 CleanTriCount = 0;
	for (uint32 f = 0; f < RawTris; ++f)
	{
		uint32 a = WI[f*3], b = WI[f*3+1], c = WI[f*3+2];
		if (a == b || b == c || a == c) continue;
		OldToCleanTri[f] = CleanTriCount++;
		CleanI.push_back(a); CleanI.push_back(b); CleanI.push_back(c);
	}

	const uint32 NV = static_cast<uint32>(WV.size());
	const uint32 NI = static_cast<uint32>(CleanI.size());
	const uint32 NT = NI / 3;
	const uint32 TargetT = (std::max)(static_cast<uint32>(NT * TargetRatio), 4u);

	if (NT <= TargetT)
	{
		Result.Vertices = WV; Result.Indices = CleanI; Result.Sections = InSections;
		// Section 재계산 (degenerate 제거로 인덱스 변경)
		goto rebuild_sections;
	}

	{
		// ── 2단계: QEM Simplification ──
		TArray<FNormalVertex> V = WV;
		TArray<uint32> I = CleanI;

		std::vector<std::vector<uint32>> VF(NV);
		std::vector<std::vector<uint32>> VN(NV);

		for (uint32 f = 0; f < NT; ++f)
		{
			uint32 i0=I[f*3], i1=I[f*3+1], i2=I[f*3+2];
			VF[i0].push_back(f); VF[i1].push_back(f); VF[i2].push_back(f);
			VecAdd(VN[i0],i1); VecAdd(VN[i0],i2);
			VecAdd(VN[i1],i0); VecAdd(VN[i1],i2);
			VecAdd(VN[i2],i0); VecAdd(VN[i2],i1);
		}

		// 경계 정점
		std::vector<bool> IsBnd(NV, false);
		{
			struct HE { uint32 A, B; };
			std::vector<HE> HEs; HEs.reserve(NT*3);
			for (uint32 f=0; f<NT; ++f)
			{
				uint32 t[3]={I[f*3],I[f*3+1],I[f*3+2]};
				for (int j=0;j<3;++j) { uint32 a=t[j],b=t[(j+1)%3]; if(a>b) std::swap(a,b); HEs.push_back({a,b}); }
			}
			std::sort(HEs.begin(), HEs.end(), [](const HE& X, const HE& Y){ return X.A<Y.A||(X.A==Y.A&&X.B<Y.B); });
			for (size_t i=0; i<HEs.size(); )
			{
				size_t j=i+1;
				while (j<HEs.size() && HEs[j].A==HEs[i].A && HEs[j].B==HEs[i].B) ++j;
				if (j-i==1) { IsBnd[HEs[i].A]=true; IsBnd[HEs[i].B]=true; }
				i=j;
			}
		}

		// Quadric 누적
		std::vector<FQuadric> Q(NV);
		for (uint32 f=0; f<NT; ++f)
		{
			const FVector& P0=V[I[f*3]].pos, &P1=V[I[f*3+1]].pos, &P2=V[I[f*3+2]].pos;
			FVector N = (P1-P0).Cross(P2-P0);
			float Len = N.Length();
			if (Len < 1e-8f) continue;
			N = N / Len;
			double a=N.X, b=N.Y, c=N.Z, d=-(a*P0.X+b*P0.Y+c*P0.Z);
			FQuadric FQ = FQuadric::FromPlane(a,b,c,d);
			Q[I[f*3]] += FQ; Q[I[f*3+1]] += FQ; Q[I[f*3+2]] += FQ;
		}

		std::vector<uint32> VVer(NV, 0);
		std::vector<bool> FA(NT, true);
		std::vector<bool> VA(NV, true);

		auto CalcCost = [&](uint32 v0, uint32 v1) -> FCandidate
		{
			FCandidate C;
			C.V0=v0; C.V1=v1;
			C.Version = VVer[v0] + VVer[v1];
			FQuadric Qs = Q[v0] + Q[v1];
			const FVector& P0=V[v0].pos, &P1=V[v1].pos;

			double ox,oy,oz;
			bool bSolved = Qs.Solve(ox,oy,oz);
			if (bSolved)
			{
				float EL = FVector::Distance(P0,P1);
				FVector Opt = {(float)ox,(float)oy,(float)oz};
				if (FVector::Distance(Opt,P0) > EL*2.0f || FVector::Distance(Opt,P1) > EL*2.0f)
					bSolved = false;
				else { C.OptPos=Opt; C.Cost=(float)Qs.Eval(ox,oy,oz); }
			}
			if (!bSolved)
			{
				FVector Mid = {(P0.X+P1.X)*0.5f,(P0.Y+P1.Y)*0.5f,(P0.Z+P1.Z)*0.5f};
				double E0=Qs.Eval(P0.X,P0.Y,P0.Z), E1=Qs.Eval(P1.X,P1.Y,P1.Z), EM=Qs.Eval(Mid.X,Mid.Y,Mid.Z);
				if (E0<=E1 && E0<=EM) { C.OptPos=P0; C.Cost=(float)E0; }
				else if (E1<=EM)      { C.OptPos=P1; C.Cost=(float)E1; }
				else                  { C.OptPos=Mid; C.Cost=(float)EM; }
			}
			if (IsBnd[v0] || IsBnd[v1]) C.Cost *= 10.0f;
			if (C.Cost < 0.0f) C.Cost = 0.0f;
			return C;
		};

		std::vector<FCandidate> Heap;
		Heap.reserve(NT * 3);
		for (uint32 v=0; v<NV; ++v)
			for (uint32 n : VN[v])
				if (n > v) Heap.push_back(CalcCost(v, n));
		std::make_heap(Heap.begin(), Heap.end(), FCandLess{});

		uint32 CurT = NT;
		uint32 MaxIter = NT * 4;

		while (CurT > TargetT && !Heap.empty() && MaxIter-- > 0)
		{
			std::pop_heap(Heap.begin(), Heap.end(), FCandLess{});
			FCandidate C = Heap.back();
			Heap.pop_back();

			if (!VA[C.V0] || !VA[C.V1]) continue;
			if (C.Version != VVer[C.V0] + VVer[C.V1]) continue;

			uint32 Keep=C.V0, Rem=C.V1;
			const FVector& NewPos = C.OptPos;

			// Flip 검사
			bool bFlip = false;
			for (int pass=0; pass<2 && !bFlip; ++pass)
			{
				uint32 Src = pass==0 ? Rem : Keep;
				uint32 Other = pass==0 ? Keep : Rem;
				for (uint32 fi : VF[Src])
				{
					if (!FA[fi]) continue;
					uint32 i0=I[fi*3], i1=I[fi*3+1], i2=I[fi*3+2];
					if (i0==Other||i1==Other||i2==Other) continue;

					FVector OP0=V[i0].pos, OP1=V[i1].pos, OP2=V[i2].pos;
					FVector OldN = (OP1-OP0).Cross(OP2-OP0);

					FVector NP0=(i0==Src)?NewPos:OP0, NP1=(i1==Src)?NewPos:OP1, NP2=(i2==Src)?NewPos:OP2;
					FVector NewN = (NP1-NP0).Cross(NP2-NP0);

					if (OldN.Dot(NewN) < 0.0f) { bFlip=true; break; }
				}
			}
			if (bFlip) continue;

			// ── Collapse Rem → Keep ──
			{
				float D0=FVector::Distance(NewPos,V[Keep].pos);
				float D1=FVector::Distance(NewPos,V[Rem].pos);
				float T = (D0+D1 > 1e-8f) ? D0/(D0+D1) : 0.5f;

				V[Keep].pos = NewPos;
				V[Keep].normal = V[Keep].normal*(1-T) + V[Rem].normal*T;
				float NLen = V[Keep].normal.Length();
				if (NLen > 1e-8f) V[Keep].normal = V[Keep].normal / NLen;

				V[Keep].color.X = V[Keep].color.X*(1-T)+V[Rem].color.X*T;
				V[Keep].color.Y = V[Keep].color.Y*(1-T)+V[Rem].color.Y*T;
				V[Keep].color.Z = V[Keep].color.Z*(1-T)+V[Rem].color.Z*T;
				V[Keep].color.W = V[Keep].color.W*(1-T)+V[Rem].color.W*T;

				V[Keep].tex.X = V[Keep].tex.X*(1-T)+V[Rem].tex.X*T;
				V[Keep].tex.Y = V[Keep].tex.Y*(1-T)+V[Rem].tex.Y*T;
			}

			Q[Keep] = Q[Keep] + Q[Rem];

			// Face 처리 — VF[Rem] 복사 후 순회 (순회 중 수정 방지)
			std::vector<uint32> RemFaces = VF[Rem];
			for (uint32 fi : RemFaces)
			{
				if (!FA[fi]) continue;
				uint32* Tri = &I[fi*3];
				bool bHasKeep = (Tri[0]==Keep||Tri[1]==Keep||Tri[2]==Keep);
				if (bHasKeep)
				{
					FA[fi]=false; CurT--;
					for (int k=0;k<3;++k) VecRemove(VF[Tri[k]], fi);
				}
				else
				{
					for (int k=0;k<3;++k) if (Tri[k]==Rem) Tri[k]=Keep;
					VecAdd(VF[Keep], fi);
				}
			}
			VF[Rem].clear();

			// 이웃 이전
			std::vector<uint32> RemNeigh = VN[Rem]; // 복사
			for (uint32 n : RemNeigh)
			{
				if (n==Keep) continue;
				VecRemove(VN[n], Rem);
				VecAdd(VN[n], Keep);
				VecAdd(VN[Keep], n);
			}
			VecRemove(VN[Keep], Rem);

			VA[Rem]=false;
			VN[Rem].clear();

			VVer[Keep]++;
			for (uint32 n : VN[Keep])
			{
				if (!VA[n]) continue;
				Heap.push_back(CalcCost(Keep, n));
				std::push_heap(Heap.begin(), Heap.end(), FCandLess{});
			}
		}

		// 결과 수집
		TArray<uint32> Remap(NV, UINT32_MAX);
		TArray<FNormalVertex> NewV;

		for (uint32 i=0; i<NV; ++i)
		{
			if (!VA[i]) continue;
			bool bUsed=false;
			for (uint32 fi : VF[i]) if (FA[fi]) { bUsed=true; break; }
			if (!bUsed) continue;
			Remap[i] = static_cast<uint32>(NewV.size());
			NewV.push_back(V[i]);
		}

		// Section별 인덱스
		const uint32 NS = static_cast<uint32>(InSections.size());

		// 원본 face → clean face → section 매핑
		TArray<uint32> CleanTriSec(NT, 0);
		for (uint32 s = 0; s < NS; ++s)
		{
			uint32 First = InSections[s].FirstIndex / 3;
			for (uint32 t = 0; t < InSections[s].NumTriangles; ++t)
			{
				uint32 OrigTri = First + t;
				if (OrigTri < RawTris && OldToCleanTri[OrigTri] != UINT32_MAX)
					CleanTriSec[OldToCleanTri[OrigTri]] = s;
			}
		}

		TArray<TArray<uint32>> SI(NS);
		for (uint32 f=0; f<NT; ++f)
		{
			if (!FA[f]) continue;
			uint32 A=Remap[I[f*3]], B=Remap[I[f*3+1]], C_=Remap[I[f*3+2]];
			if (A==UINT32_MAX||B==UINT32_MAX||C_==UINT32_MAX) continue;
			if (A==B||B==C_||A==C_) continue;
			uint32 s = CleanTriSec[f];
			SI[s].push_back(A); SI[s].push_back(B); SI[s].push_back(C_);
		}

		TArray<uint32> NewI;
		TArray<FStaticMeshSection> NewS;
		for (uint32 s=0; s<NS; ++s)
		{
			if (SI[s].empty()) continue;
			FStaticMeshSection Sec;
			Sec.MaterialIndex = InSections[s].MaterialIndex;
			Sec.MaterialSlotName = InSections[s].MaterialSlotName;
			Sec.FirstIndex = static_cast<uint32>(NewI.size());
			Sec.NumTriangles = static_cast<uint32>(SI[s].size())/3;
			NewI.insert(NewI.end(), SI[s].begin(), SI[s].end());
			NewS.push_back(Sec);
		}

		Result.Vertices = std::move(NewV);
		Result.Indices = std::move(NewI);
		Result.Sections = std::move(NewS);
		return Result;
	}

rebuild_sections:
	{
		// degenerate 제거 후 Section 재계산
		const uint32 NS = static_cast<uint32>(InSections.size());
		TArray<uint32> CleanTriSec(NT, 0);
		for (uint32 s = 0; s < NS; ++s)
		{
			uint32 First = InSections[s].FirstIndex / 3;
			for (uint32 t = 0; t < InSections[s].NumTriangles; ++t)
			{
				uint32 OrigTri = First + t;
				if (OrigTri < RawTris && OldToCleanTri[OrigTri] != UINT32_MAX)
					CleanTriSec[OldToCleanTri[OrigTri]] = s;
			}
		}

		TArray<TArray<uint32>> SI(NS);
		for (uint32 f = 0; f < NT; ++f)
		{
			SI[CleanTriSec[f]].push_back(CleanI[f*3]);
			SI[CleanTriSec[f]].push_back(CleanI[f*3+1]);
			SI[CleanTriSec[f]].push_back(CleanI[f*3+2]);
		}

		Result.Indices.clear();
		Result.Sections.clear();
		for (uint32 s = 0; s < NS; ++s)
		{
			if (SI[s].empty()) continue;
			FStaticMeshSection Sec;
			Sec.MaterialIndex = InSections[s].MaterialIndex;
			Sec.MaterialSlotName = InSections[s].MaterialSlotName;
			Sec.FirstIndex = static_cast<uint32>(Result.Indices.size());
			Sec.NumTriangles = static_cast<uint32>(SI[s].size()) / 3;
			Result.Indices.insert(Result.Indices.end(), SI[s].begin(), SI[s].end());
			Result.Sections.push_back(Sec);
		}
		return Result;
	}
}
