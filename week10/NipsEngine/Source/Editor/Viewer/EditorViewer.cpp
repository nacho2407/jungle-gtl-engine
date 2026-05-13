#include "EditorViewer.h"
#include "EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Selection/SelectionManager.h"
#include "Component/GizmoComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformProxy.h"
#include "Core/ResourceManager.h"
#include "Engine/Geometry/Ray.h"
#include "GameFramework/PrimitiveActors.h"
#include "Math/Vector4.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "Component/PostProcess/Light/AmbientLightComponent.h"

namespace
{
float DistanceSquaredRaySegment(const FRay& Ray, const FVector& SegmentStart, const FVector& SegmentEnd, float& OutRayT)
{
    const FVector RayDir = Ray.Direction.GetSafeNormal();
    const FVector SegmentDir = SegmentEnd - SegmentStart;
    const FVector RayToSegment = Ray.Origin - SegmentStart;

    constexpr float Epsilon = 1.0e-6f;
    const float RayLenSq = std::max(FVector::DotProduct(RayDir, RayDir), Epsilon);
    const float SegmentLenSq = FVector::DotProduct(SegmentDir, SegmentDir);

    float RayT = 0.0f;
    float SegmentT = 0.0f;

    if (SegmentLenSq <= Epsilon)
    {
        RayT = std::max(0.0f, -FVector::DotProduct(RayDir, RayToSegment) / RayLenSq);
    }
    else
    {
        const float RaySegmentDot = FVector::DotProduct(RayDir, SegmentDir);
        const float RayOriginDot = FVector::DotProduct(RayDir, RayToSegment);
        const float SegmentOriginDot = FVector::DotProduct(SegmentDir, RayToSegment);
        const float Denom = RayLenSq * SegmentLenSq - RaySegmentDot * RaySegmentDot;

        if (std::abs(Denom) > Epsilon)
        {
            RayT = std::max(0.0f, (RaySegmentDot * SegmentOriginDot - RayOriginDot * SegmentLenSq) / Denom);
        }

        SegmentT = (RaySegmentDot * RayT + SegmentOriginDot) / SegmentLenSq;
        if (SegmentT < 0.0f)
        {
            SegmentT = 0.0f;
            RayT = std::max(0.0f, -RayOriginDot / RayLenSq);
        }
        else if (SegmentT > 1.0f)
        {
            SegmentT = 1.0f;
            RayT = std::max(0.0f, (RaySegmentDot - RayOriginDot) / RayLenSq);
        }
    }

    const FVector ClosestOnRay = Ray.Origin + RayDir * RayT;
    const FVector ClosestOnSegment = SegmentStart + SegmentDir * SegmentT;
    OutRayT = RayT;
    return FVector::DistSquared(ClosestOnRay, ClosestOnSegment);
}

float DistanceSquaredPointSegment2D(
    float PointX,
    float PointY,
    float SegmentStartX,
    float SegmentStartY,
    float SegmentEndX,
    float SegmentEndY)
{
    const float SegmentX = SegmentEndX - SegmentStartX;
    const float SegmentY = SegmentEndY - SegmentStartY;
    const float SegmentLenSq = SegmentX * SegmentX + SegmentY * SegmentY;
    if (SegmentLenSq <= 1.0e-6f)
    {
        const float DX = PointX - SegmentStartX;
        const float DY = PointY - SegmentStartY;
        return DX * DX + DY * DY;
    }

    const float T = std::clamp(
        ((PointX - SegmentStartX) * SegmentX + (PointY - SegmentStartY) * SegmentY) / SegmentLenSq,
        0.0f,
        1.0f);
    const float ClosestX = SegmentStartX + SegmentX * T;
    const float ClosestY = SegmentStartY + SegmentY * T;
    const float DX = PointX - ClosestX;
    const float DY = PointY - ClosestY;
    return DX * DX + DY * DY;
}

bool ProjectWorldToViewport(
    const FViewportCamera& Camera,
    const FVector& WorldPos,
    float ViewportWidth,
    float ViewportHeight,
    float& OutViewportX,
    float& OutViewportY,
    float& OutDepth)
{
    const FVector4 Clip = FMatrix::Identity.TransformVector4(FVector4(WorldPos, 1.0f), Camera.GetViewProjectionMatrix());
    if (MathUtil::IsNearlyZero(Clip.W))
    {
        return false;
    }

    const float InvW = 1.0f / Clip.W;
    const float NdcX = Clip.X * InvW;
    const float NdcY = Clip.Y * InvW;
    const float NdcZ = Clip.Z * InvW;
    if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f)
    {
        return false;
    }

    OutViewportX = (NdcX * 0.5f + 0.5f) * ViewportWidth;
    OutViewportY = (1.0f - (NdcY * 0.5f + 0.5f)) * ViewportHeight;
    OutDepth = NdcZ;
    return true;
}
}

void FEditorViewer::Init(
    FWindowsWindow* InWindow,
    UEditorEngine* InEditor,
    UWorld* InWorld,
    FSelectionManager* InSelectionManager)
{
    Viewport.SetClient(&Client);

    Client.Initialize(InWindow, InEditor);
    Client.SetWorld(InWorld);
    Client.SetGizmo(InSelectionManager->GetGizmo());
    Client.SetSelectionManager(InSelectionManager);
    Client.SetSceneEditingShortcutsEnabled(false);

    Client.SetViewport(&Viewport);
    Client.SetState(&Viewport.GetState());
    Client.SetBonePickHandler([this](float LocalX, float LocalY)
    {
        return HandleBonePick(LocalX, LocalY);
    });

    Client.SetViewportType(EEditorViewportType::EVT_Perspective);
    Client.CreateCamera();
    Client.ApplyCameraMode();
    Viewport.GetState().ViewMode = EViewMode::Lit_BlinnPhong;
    Viewport.GetState().LightCullMode = ELightCullMode::None;

	FViewportRect Rect = { 0, 0, 300, 300 };
    Viewport.SetRect(Rect);

    ViewTarget = InWorld->SpawnActor<ASkeletalMeshActor>();
    ViewTarget->InitDefaultComponents();
    ViewTarget->SetFName(FName("ViewerActor"));
    ViewTarget->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

    ADirectionalLightActor* DirectionalLight = InWorld->SpawnActor<ADirectionalLightActor>();
    if (DirectionalLight)
    {
        DirectionalLight->InitDefaultComponents();
        DirectionalLight->SetFName(FName("Viewer Directional Light"));
        DirectionalLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
        DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
    }

    AAmbientLightActor* AmbientLight = InWorld->SpawnActor<AAmbientLightActor>();
    if (AmbientLight)
    {
        AmbientLight->InitDefaultComponents();
        AmbientLight->SetFName(FName("Viewer Ambient Light"));
        AmbientLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
    }
    
    AmbientLight->FindComponent<UAmbientLightComponent>()->Intensity = 0.7f;

    InWorld->SyncSpatialIndex();
}

void FEditorViewer::Shutdown()
{
    // UEditorEngine::Shutdown 흐름:
    //   CloseScene() (ViewTarget actor 포함 destroy) → ... → Viewer.Shutdown()
    // 이 시점에 ViewTarget raw 포인터는 dangling이므로 RemoveComponent를 호출하면 안 됨.
    // SocketPreview 컴포넌트들은 ViewTarget actor 소멸자가 이미 함께 destroy함.
    // 우리는 단순히 map만 비우고 포인터를 null로 잡는다.
    SocketPreviewMeshes.clear();
    ViewTarget = nullptr;
    Client.SetBonePickHandler(nullptr);

    Viewport.SetClient(nullptr);
}

void FEditorViewer::Tick(float DeltaTime)
{
    Client.Tick(DeltaTime);
}

void FEditorViewer::SelectBone(int32 BoneIndex)
{
    if (!ViewTarget)
    {
        return;
    }

    USkeletalMeshComponent* SkelComp = ViewTarget->GetSkeletalMeshComponent();
    USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
    if (!SkelComp || !Mesh || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Mesh->GetBones().size()))
    {
        return;
    }

    SelectedBoneIndex = BoneIndex;
    SelectedSocketIndex = -1;

    if (FSelectionManager* SelectionManager = Client.GetSelectionManager())
    {
        SelectionManager->Select(ViewTarget);
    }

    if (UGizmoComponent* Gizmo = Client.GetGizmo())
    {
        Gizmo->SetProxy(std::make_shared<FBoneTransformProxy>(SkelComp, SelectedBoneIndex));
        Gizmo->SetSelectedActors(nullptr);

        FMatrix LocalTransform = SkelComp->GetBoneLocalTransform(SelectedBoneIndex);
        FVector DummyLocation;
        FVector DummyScale;
        FMatrix RotationMatrix;
        LocalTransform.Decompose(DummyLocation, RotationMatrix, DummyScale);
        CachedRotation = RotationMatrix.GetEuler();
    }
}

void FEditorViewer::SelectSocket(int32 SocketIndex)
{
    if (!ViewTarget)
    {
        return;
    }

    USkeletalMeshComponent* SkelComp = ViewTarget->GetSkeletalMeshComponent();
    USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
    FSkeletalMesh* MeshData = Mesh ? Mesh->GetMeshData() : nullptr;
    if (!SkelComp || !MeshData || SocketIndex < 0 || SocketIndex >= static_cast<int32>(MeshData->Sockets.size()))
    {
        return;
    }

    const FName SocketName = MeshData->Sockets[SocketIndex].Name;
    SelectedBoneIndex = -1;
    SelectedSocketIndex = SocketIndex;

    if (FSelectionManager* SelectionManager = Client.GetSelectionManager())
    {
        SelectionManager->Select(ViewTarget);
    }

    if (UGizmoComponent* Gizmo = Client.GetGizmo())
    {
        Gizmo->SetProxy(std::make_shared<FSocketTransformProxy>(SkelComp, SocketName));
        Gizmo->SetSelectedActors(nullptr);
    }
}

void FEditorViewer::ClearSelection()
{
    SelectedBoneIndex = -1;
    SelectedSocketIndex = -1;
    CachedRotation = FVector::ZeroVector;

    if (FSelectionManager* SelectionManager = Client.GetSelectionManager())
    {
        SelectionManager->ClearSelection();
        return;
    }

    if (UGizmoComponent* Gizmo = Client.GetGizmo())
    {
        Gizmo->Deactivate();
    }
}

void FEditorViewer::NotifySocketDeleted(int32 SocketIndex)
{
    if (SelectedSocketIndex == SocketIndex)
    {
        ClearSelection();
    }
    else if (SelectedSocketIndex > SocketIndex)
    {
        --SelectedSocketIndex;
    }
}

bool FEditorViewer::HandleBonePick(float LocalX, float LocalY)
{
    int32 PickedBoneIndex = -1;
    if (!TryPickBone(LocalX, LocalY, PickedBoneIndex))
    {
        ClearSelection();
        return true;
    }

    SelectBone(PickedBoneIndex);
    return true;
}

bool FEditorViewer::TryPickBone(float LocalX, float LocalY, int32& OutBoneIndex) const
{
    OutBoneIndex = -1;
    if (!ViewTarget || !Client.GetCamera())
    {
        return false;
    }

    USkeletalMeshComponent* SkelComp = ViewTarget->GetSkeletalMeshComponent();
    USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
    if (!SkelComp || !Mesh || !Mesh->HasValidMeshData())
    {
        return false;
    }

    const FViewportRect& Rect = Viewport.GetRect();
    if (Rect.Width <= 0 || Rect.Height <= 0)
    {
        return false;
    }

    SkelComp->EnsureSkinningUpdated();

    const FViewportCamera& Camera = *Client.GetCamera();
    const float ViewportWidth = static_cast<float>(Rect.Width);
    const float ViewportHeight = static_cast<float>(Rect.Height);

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();
    float BestScore = FLT_MAX;
    int32 BestBoneIndex = -1;

    constexpr float PickRadiusPixels = 12.0f;
    constexpr float PickRadiusPixelsSq = PickRadiusPixels * PickRadiusPixels;

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(Bones.size()))
        {
            continue;
        }

        const FVector SegmentStart = SkelComp->GetBoneWorldMatrix(ParentIndex).GetTranslation();
        const FVector SegmentEnd = SkelComp->GetBoneWorldMatrix(BoneIndex).GetTranslation();
        const float SegmentLength = FVector::Dist(SegmentStart, SegmentEnd);
        if (SegmentLength <= 1.0e-4f)
        {
            continue;
        }

        float StartX = 0.0f;
        float StartY = 0.0f;
        float StartDepth = 0.0f;
        float EndX = 0.0f;
        float EndY = 0.0f;
        float EndDepth = 0.0f;
        if (!ProjectWorldToViewport(Camera, SegmentStart, ViewportWidth, ViewportHeight, StartX, StartY, StartDepth) ||
            !ProjectWorldToViewport(Camera, SegmentEnd, ViewportWidth, ViewportHeight, EndX, EndY, EndDepth))
        {
            continue;
        }

        const float DistanceSq = DistanceSquaredPointSegment2D(LocalX, LocalY, StartX, StartY, EndX, EndY);
        if (DistanceSq > PickRadiusPixelsSq)
        {
            continue;
        }

        const float DepthScore = std::min(StartDepth, EndDepth);
        const float Score = DistanceSq + DepthScore * 0.001f;
        if (Score < BestScore)
        {
            BestScore = Score;
            BestBoneIndex = BoneIndex;
        }
    }

    if (BestBoneIndex >= 0)
    {
        OutBoneIndex = BestBoneIndex;
        return true;
    }

    const FRay Ray = Camera.DeprojectScreenToWorld(LocalX, LocalY, ViewportWidth, ViewportHeight);
    BestScore = FLT_MAX;

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(Bones.size()))
        {
            continue;
        }

        const FVector SegmentStart = SkelComp->GetBoneWorldMatrix(ParentIndex).GetTranslation();
        const FVector SegmentEnd = SkelComp->GetBoneWorldMatrix(BoneIndex).GetTranslation();
        const float SegmentLength = FVector::Dist(SegmentStart, SegmentEnd);
        if (SegmentLength <= 1.0e-4f)
        {
            continue;
        }

        float RayT = 0.0f;
        const float DistanceSq = DistanceSquaredRaySegment(Ray, SegmentStart, SegmentEnd, RayT);
        const float PickRadius = std::clamp(SegmentLength * 0.18f, 1.0f, 12.0f);
        if (DistanceSq > PickRadius * PickRadius)
        {
            continue;
        }

        const float Score = DistanceSq + RayT * 0.0001f;
        if (Score < BestScore)
        {
            BestScore = Score;
            BestBoneIndex = BoneIndex;
        }
    }

    if (BestBoneIndex < 0)
    {
        return false;
    }

    OutBoneIndex = BestBoneIndex;
    return true;
}

void FEditorViewer::SetSocketPreviewMesh(const FName& SocketName, const FString& StaticMeshPath)
{
    if (!ViewTarget) return;

    // 같은 socket에 이미 preview가 있으면 먼저 제거 (덮어쓰기 동작)
    ClearSocketPreview(SocketName);

    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh(StaticMeshPath);
    if (!Mesh) return;

    UStaticMeshComponent* Preview = ViewTarget->AddComponent<UStaticMeshComponent>();
    if (!Preview) return;

    // 휘발성 + 에디터 전용. Scene 저장에 안 들어가고, 게임 빌드 render에 안 잡힘.
    Preview->SetTransient(true);
    Preview->SetEditorOnly(true);
    Preview->SetStaticMesh(Mesh);

    if (USkeletalMeshComponent* SkComp = ViewTarget->GetSkeletalMeshComponent())
    {
        Preview->AttachToComponent(SkComp, SocketName);
    }

    SocketPreviewMeshes[SocketName] = Preview;
}

void FEditorViewer::ClearSocketPreview(const FName& SocketName)
{
    auto It = SocketPreviewMeshes.find(SocketName);
    if (It == SocketPreviewMeshes.end()) return;

    if (ViewTarget && It->second)
    {
        ViewTarget->RemoveComponent(It->second);
        // RemoveComponent가 UObjectManager::DestroyObject까지 호출 — 추가 cleanup 불필요.
    }
    SocketPreviewMeshes.erase(It);
}

void FEditorViewer::ClearAllSocketPreviews()
{
    if (ViewTarget)
    {
        for (auto& [Name, Comp] : SocketPreviewMeshes)
        {
            if (Comp)
            {
                ViewTarget->RemoveComponent(Comp);
            }
        }
    }
    SocketPreviewMeshes.clear();
}

UStaticMeshComponent* FEditorViewer::FindPreviewMesh(const FName& SocketName) const
{
    auto It = SocketPreviewMeshes.find(SocketName);
    return It != SocketPreviewMeshes.end() ? It->second : nullptr;
}

void FEditorViewer::ChangeTarget(const FString& InFileName)
{
    FileName = InFileName;
    USkeletalMesh* SkelMesh = FResourceManager::Get().LoadSkeletalMesh(InFileName.c_str());
    if (SkelMesh)
	    ViewTarget->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
}
