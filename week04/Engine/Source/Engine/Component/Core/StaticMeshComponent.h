#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

class UStaticMesh;

namespace Engine::Component
{
    class ENGINE_API UStaticMeshComponent : public UPrimitiveComponent
    {
      public:
        DECLARE_RTTI(UStaticMeshComponent, UPrimitiveComponent)
        UStaticMeshComponent();
        virtual ~UStaticMeshComponent() override;

        virtual EBasicMeshType GetBasicMeshType() const override;

        // Editor 속성 노출
        virtual void DescribeProperties(FComponentPropertyBuilder& Builder) override;

        // Scene 데이터 저장 및 복원 (JSON 연동)
        virtual void Serialize(bool bIsLoading, void* JsonHandle);

        // Asset 설정 및 반환
        void         SetStaticMesh(UStaticMesh* InStaticMesh) { StaticMesh = InStaticMesh; }
        UStaticMesh* GetStaticMesh() const { return StaticMesh; }

      protected:
        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        virtual Geometry::FAABB GetLocalAABB() const override;

      private:
        // Asset 선택용 Path 제어 함수
        FString GetMeshPath() const;
        void    SetMeshPath(const FString& InPath);

      private:
        // Component가 참조하는 StaticMeshAsset
        UStaticMesh* StaticMesh = nullptr;
    };
} // namespace Engine::Component