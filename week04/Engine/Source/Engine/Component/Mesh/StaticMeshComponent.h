#pragma once

#include "Engine/Component/Core/MeshComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Asset
{
    class UStaticMesh;
}

namespace Engine::Component
{
    /**
     * @brief 정적 메시(Static Mesh)를 렌더링하기 위한 컴포넌트입니다.
     */
    class ENGINE_API UStaticMeshComponent : public UMeshComponent
    {
      public:
        DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

        UStaticMeshComponent();
        virtual ~UStaticMeshComponent() override;

        virtual EBasicMeshType GetBasicMeshType() const override;
        virtual void           DescribeProperties(FComponentPropertyBuilder& Builder) override;
        virtual void           Serialize(bool bIsLoading, void* JsonHandle) override;

        /** 에셋 참조 해결 (언리얼의 PostLoad와 유사한 역할) */
        virtual void           ResolveAssetReferences(class UAssetManager* InAssetManager) override;

        void               SetStaticMesh(Asset::UStaticMesh* InStaticMesh);
        Asset::UStaticMesh* GetStaticMesh() const { return StaticMesh; }

        // 에디터 및 외부 시스템에서 접근할 수 있도록 경로 관련 함수를 제공합니다.
        FString         GetMeshPath() const;
        void            SetMeshPath(const FString& InPath);
        const FString&  GetSubMaterialName(uint32 Index) const;
        void            SetSubMaterialName(uint32 Index, const FString& InSubMaterialName);
        TArray<FString> GetAvailableSubMaterialNames() const;

        virtual bool ShouldShowInDetailsTree() const override { return true; }

        /** 머티리얼 접근 오버라이드 (컴포넌트에 없으면 에셋의 것을 반환) */
        virtual Asset::UMaterialInterface* GetMaterial(uint32 Index) const override;
        virtual uint32                     GetNumMaterials() const override;

      protected:
        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        virtual Geometry::FAABB GetLocalAABB() const override;

      private:
        Asset::UStaticMesh* StaticMesh = nullptr;
        TArray<FString>     OverrideSubMaterialNames;
        FString             PendingMeshPath = ""; // 아직 로드되지 않은 에셋 경로
    };
} // namespace Engine::Component
