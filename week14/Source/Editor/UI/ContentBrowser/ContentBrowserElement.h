#pragma once
#include "Core/Types/ClassTypes.h"
#include "Editor/UI/ContentBrowser/ContentBrowserContext.h"
#include "Editor/UI/Util/EditorMaterialThumbnailManager.h"
#include "Editor/UI/Util/EditorMeshThumbnailManager.h"
#include "ContentItem.h"
#include <d3d11.h>
#include <shellapi.h>


class ContentBrowserElement : public std::enable_shared_from_this<ContentBrowserElement>
{
public:
	virtual ~ContentBrowserElement() = default;
	bool RenderSelectSpace(ContentBrowserContext& Context);
	virtual void Render(ContentBrowserContext& Context);
	/**
	 * @brief Content Browser 리스트 모드의 한 행을 렌더링합니다
	 *
	 * @param Context Content Browser 렌더링 컨텍스트
	 */
	virtual void RenderListRow(ContentBrowserContext& Context);
	virtual void RenderDetail();

	virtual void RenderContextMenu(ContentBrowserContext& Context) {}

	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

	void SetMeshThumbnailRequest(const FString& AssetPath, EMeshThumbnailType Type)
	{
		MeshThumbnailAssetPath = AssetPath;
		MeshThumbnailType = Type;
		bUseMeshThumbnail = true;
	}

	void SetMaterialThumbnailRequest(const FString& AssetPath)
	{
		MaterialThumbnailAssetPath = AssetPath;
		bUseMaterialThumbnail = true;
	}

	std::wstring GetFileName() { return ContentItem.Path.filename(); }

	// Rename UI 등 외부 코드가 path / stem / 디렉토리 여부를 알 수 있게.
	const FContentItem& GetContentItem() const { return ContentItem; }

	// 같은 디렉토리 안에서 NewStem 으로 rename. 파일은 확장자 유지, 디렉토리는 그대로 이름 변경.
	// 중복이 있으면 false + OutError. 성공 시 ContentItem.Path/Name 갱신 후 true.
	// 실제 .uasset 안의 AssetPathFileName 등 캐시는 별도로 reload 필요 (다음 refresh).
	bool RenameTo(const FString& NewStem, FString* OutError = nullptr);
	bool DeleteFromDisk(FString* OutError = nullptr);

protected:
	FString EllipsisText(const FString& text, float maxWidth);

	virtual FString GetDisplayName() const;
	virtual const char* GetTypeLabel() const { return ""; }
	virtual const char* GetDragItemType() { return "ParkSangHyeok"; }

	virtual uint32 GetAccentColor() const { return 0; }

	virtual void OnLeftClicked(ContentBrowserContext& Context) { (void)Context; };
	virtual void OnDoubleLeftClicked(ContentBrowserContext& Context) { ShellExecuteW(nullptr, L"open", ContentItem.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL); };
	virtual void OnDrag(ContentBrowserContext& Context) { (void)Context; }

protected:
	ID3D11ShaderResourceView* Icon = nullptr;
	FContentItem ContentItem;
	bool bIsSelected = false;

	bool bUseMeshThumbnail = false;
	FString MeshThumbnailAssetPath;
	EMeshThumbnailType MeshThumbnailType = EMeshThumbnailType::StaticMesh;

	bool bUseMaterialThumbnail = false;
	FString MaterialThumbnailAssetPath;
};

class DirectoryElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;
};

class SceneElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;
};

class PrefabElement final : public ContentBrowserElement
{
public:
	virtual const char* GetDragItemType() override { return "PrefabContentItem"; }

protected:
	const char* GetTypeLabel() const override { return "Prefab"; }
	uint32 GetAccentColor() const override { return IM_COL32(110, 210, 180, 255); }
};

class ObjectElement : public ContentBrowserElement
{
public:
	void RenderContextMenu(ContentBrowserContext& Context) override;
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

	virtual const char* GetDragItemType() override { return "ObjectContentItem"; }

protected:
	const char* GetTypeLabel() const override { return "Static Mesh"; }
	uint32 GetAccentColor() const override { return IM_COL32(88, 160, 230, 255); }
};

class ObjSourceElement final : public ObjectElement
{
protected:
	const char* GetTypeLabel() const override { return "OBJ Source"; }
	uint32 GetAccentColor() const override { return IM_COL32(120, 150, 180, 255); }
};

class FloatCurveElement final : public ContentBrowserElement
{
public:
	virtual const char* GetDragItemType() override { return "FloatCurveContentItem"; }
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Float Curve"; }
	uint32 GetAccentColor() const override { return IM_COL32(90, 190, 120, 255); }
};

class CameraShakeElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Camera Shake"; }
	uint32 GetAccentColor() const override { return IM_COL32(230, 150, 75, 255); }
};

class AnimGraphElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Anim Graph"; }
	uint32 GetAccentColor() const override { return IM_COL32(200, 110, 200, 255); }
};

class MeshElement : public ContentBrowserElement
{
public:
	void RenderContextMenu(ContentBrowserContext& Context) override;

	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Skeletal Mesh"; }
	uint32 GetAccentColor() const override { return IM_COL32(126, 140, 255, 255); }
};

class FbxSourceElement final : public MeshElement
{
protected:
	const char* GetTypeLabel() const override { return "FBX Source"; }
	uint32 GetAccentColor() const override { return IM_COL32(150, 150, 170, 255); }
};

class AnimationElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Animation"; }
	uint32 GetAccentColor() const override { return IM_COL32(255, 180, 90, 255); }
};

class SkeletonElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Skeleton"; }
	uint32 GetAccentColor() const override { return IM_COL32(180, 130, 255, 255); }
};

class PhysicsAssetElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Physics Asset"; }
	uint32 GetAccentColor() const override { return IM_COL32(100, 190, 210, 255); }
};

class ParticleSystemElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Particle System"; }
	uint32 GetAccentColor() const override { return IM_COL32(255, 120, 120, 255); }
};

class PhysicalMaterialElement final : public ContentBrowserElement
{
protected:
	const char* GetTypeLabel() const override { return "Physical Material"; }
	uint32 GetAccentColor() const override { return IM_COL32(140, 190, 255, 255); }
};

class ImageElement final : public ContentBrowserElement
{
public:
	const char* GetDragItemType() override { return "PNGElement"; }

protected:
	const char* GetTypeLabel() const override { return "Image"; }
	uint32 GetAccentColor() const override { return IM_COL32(120, 190, 255, 255); }
};

class RmlElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "RML UI"; }
	uint32 GetAccentColor() const override { return IM_COL32(120, 210, 255, 255); }
};

#include "Editor/UI/Panel/EditorMaterialInspector.h"
class MaterialElement final : public ContentBrowserElement
{
public:
	virtual void OnDoubleLeftClicked(ContentBrowserContext& Context) override;
	virtual const char* GetDragItemType() override { return "MaterialContentItem"; }

protected:
	const char* GetTypeLabel() const override;
	uint32 GetAccentColor() const override;

private:
	FEditorMaterialInspector MaterialInspector;
};
