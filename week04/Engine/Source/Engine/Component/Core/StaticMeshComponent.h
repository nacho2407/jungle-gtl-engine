#pragma once

#include "Engine/Component/Core/MeshComponent.h"
#include "Renderer/Types/BasicMeshType.h"

class UStaticMesh;

namespace Engine::Component
{
    /**
     * @brief 정적 메시를 월드에 렌더링하기 위한 컴포넌트입니다.
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

        /** 에셋 할당 */
        void         SetStaticMesh(UStaticMesh* InStaticMesh);
        UStaticMesh* GetStaticMesh() const { return StaticMesh; }

        // 에디터 및 외부 시스템에서 접근할 수 있도록 public으로 공개합니다.
        FString GetMeshPath() const;
        void    SetMeshPath(const FString& InPath);

        // 에디터 및 외부 시스템에서 접근할 수 있도록 public으로 공개합니다.
        FString GetMeshPath() const;
        void    SetMeshPath(const FString& InPath);

         virtual bool ShouldShowInDetailsTree() const override { return true; }

      protected:
        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        virtual Geometry::FAABB GetLocalAABB() const override;

      private:
        UStaticMesh* StaticMesh = nullptr;
        FString      PendingMeshPath = ""; // 아직 로드되지 않은 에셋 경로
    };
} // namespace Engine::Component
