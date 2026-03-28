#pragma once

#include "Engine/Component/Core/MeshComponent.h"
#include "Renderer/Types/BasicMeshType.h"

class UStaticMesh;

namespace Engine::Component
{
    class ENGINE_API UStaticMeshComponent : public UMeshComponent
    {
      public:
        DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

        UStaticMeshComponent();
        virtual ~UStaticMeshComponent() override;

        virtual EBasicMeshType GetBasicMeshType() const override;
        virtual void           DescribeProperties(FComponentPropertyBuilder& Builder) override;
        virtual void           Serialize(bool bIsLoading, void* JsonHandle) override;

        void         SetStaticMesh(UStaticMesh* InStaticMesh);
        UStaticMesh* GetStaticMesh() const { return StaticMesh; }

        virtual bool ShouldShowInDetailsTree() const override { return true; }

      protected:
        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        virtual Geometry::FAABB GetLocalAABB() const override;

      private:
        FString GetMeshPath() const;
        void    SetMeshPath(const FString& InPath);

      private:
        UStaticMesh* StaticMesh = nullptr;
    };
} // namespace Engine::Component
