#include "BillboardSubUVComponent.h"

#include <algorithm>

#include "Component/CameraComponent.h"
#include "EngineCore/ResourceManager.h"
#include "Scene.h"

UBillboardSubUVComponent::UBillboardSubUVComponent()
{
}

UBillboardSubUVComponent::~UBillboardSubUVComponent()
{
	if (RenderObj != nullptr)
	{
		if (Scene != nullptr)
		{
			Scene->Level.UnregisterRenderObject(RenderObj);
		}
		delete RenderObj;
	}
}

void UBillboardSubUVComponent::OnComponentAdded()
{
	RenderObj = CreateRenderObject();
	if (RenderObj != nullptr && Scene != nullptr)
	{
		Scene->Level.RegisterRenderObject(RenderObj);
	}
}

void UBillboardSubUVComponent::Update(float DeltaTime)
{
	CurrentTime += DeltaTime;
	UpdateRenderObject();
}

void UBillboardSubUVComponent::SetSize(float InWidth, float InHeight)
{
	Width = InWidth;
	Height = InHeight;
	UpdateRenderObject();
}

void UBillboardSubUVComponent::SetGrid(int32 InRows, int32 InColumns, int32 InFrameCount)
{
	Rows = (std::max)(1, InRows);
	Columns = (std::max)(1, InColumns);
	FrameCount = (std::max)(1, InFrameCount);
	UpdateRenderObject();
}

void UBillboardSubUVComponent::SetPlayback(float InFramesPerSecond, bool bInLoop)
{
	FramesPerSecond = (std::max)(0.0f, InFramesPerSecond);
	bLoop = bInLoop;
	UpdateRenderObject();
}

RenderObject* UBillboardSubUVComponent::CreateRenderObject()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	RenderObject* renderObject = new RenderObject();
	renderObject->Geometry = &resourceManager->GetGeometry("BillboardQuad");
	renderObject->Material = &resourceManager->GetMaterial(L"Shader/ShaderSubUV.hlsl");
	renderObject->ShowFlag = EEngineShowFlags::SF_BillboardText;

	//SubUV는 현재 평면이므로 어디에서든 볼 수 있도록 Culling을 비활성화해뒀습니다.
	renderObject->bDepthEnabled = true;
	renderObject->bDisableCulling = true;

	RenderObj = renderObject;
	UpdateRenderObject();
	return renderObject;
}

void UBillboardSubUVComponent::UpdateRenderObject()
{
	if (RenderObj == nullptr)
	{
		return;
	}

	const int32 totalFrames = (std::max)(1, (std::min)(FrameCount, Rows * Columns));
	const int32 frameIndex = (std::min)(GetCurrentFrameIndex(), totalFrames - 1);
	const int32 column = frameIndex % Columns;
	const int32 row = frameIndex / Columns;

	//col, row를 이용해 자른 프레임에서의 UV좌표를 계산합니다.
	//UV좌표의 범위가 0~1이므로 그 비율을 계산합니다.
	const float uvScaleX = 1.0f / static_cast<float>(Columns);
	const float uvScaleY = 1.0f / static_cast<float>(Rows);
	RenderObj->UVTransform = FVector4(
		uvScaleX,
		uvScaleY,
		column * uvScaleX,
		row * uvScaleY);

	const UCameraComponent* camera = UCameraComponent::GetMainCamera();
	// 카메라가 없는 경우를 위한 기본 방향.
	FVector forward = FVector::Forward();
	FVector right = FVector::Right();
	FVector up = FVector::Up();
	if (camera != nullptr)
	{
		forward = camera->Direction.Normalize();
		right = camera->SideDirection.Normalize();
		up = camera->UpDirection.Normalize();
	}

	const FVector location = GetRelativeLocation();
	FMatrix world = FMatrix::IdentityMatrix();

	// 빌보드용 월드 행렬 구성, 항상 카메라를 바라보도록 축을 설정합니다.
	// view matrix와 형태가 유사하지만 translation 부분은 오브젝트의 위치를 사용하며 world matrix로 사용됩니다.
	// 
	// forward를 사용해 빌보드가 바라보는 방향을 결정합니다.
	world[0][0] = forward.x;
	world[0][1] = forward.y;
	world[0][2] = forward.z;

	// 빌보드의 로컬 X축 방향과 빌보드의 로컬 Y축 방향으로의 회전입니다.
	// Width를 곱해 빌보드의 가로 크기, Height를 곱해 빌보드의 세로 크기를 함께 반영합니다.
	world[1][0] = right.x * Width;
	world[1][1] = right.y * Width;
	world[1][2] = right.z * Width;
	world[2][0] = up.x * Height;
	world[2][1] = up.y * Height;
	world[2][2] = up.z * Height;

	// 위에서 카메라를 향하는 방향을 가져왔으니 여기서는 자기 자신의 위치를 그대로 사용합니다.
	world[3][0] = location.x;
	world[3][1] = location.y;
	world[3][2] = location.z;

	RenderObj->World = world;
	RenderObj->bIsSelected = bIsSelected; //렌더링에 포함시킵니다
}

int32 UBillboardSubUVComponent::GetCurrentFrameIndex() const
{
	const int32 totalFrames = (std::max)(1, (std::min)(FrameCount, Rows * Columns));
	if (FramesPerSecond <= 0.0f)
	{
		return 0;
	}

	const int32 frameIndex = (int32)(CurrentTime * FramesPerSecond);
	if (bLoop)
	{
		return frameIndex % totalFrames;
	}

	return (std::min)(frameIndex, totalFrames - 1);
}

REGISTER_CLASS(UBillboardSubUVComponent);
