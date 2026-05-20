// [UHT generated source - do not edit]
#include "Engine/Component/StaticMeshComponent.h"
#include "ReflectionSystem/ReflectionDatabase.h"
#include "ReflectionSystem/ReflectionUtils.h"
#include "ThirdParty/sol/sol.hpp"

void Register_UStaticMeshComponent()
{
    FClassInfo& info = UStaticMeshComponent::StaticClassInfo;
    info.Properties.clear();
    info.ReflectedProperties.clear();
    info.GcPointerOffsets.clear();
    info.Functions.clear();
    info.ClassName = "UStaticMeshComponent";
    info.ParentClassName = "UMeshComponent";
    {
        FObjectProperty* prop = new FObjectProperty();
        prop->ReferenceKind = EObjectReferenceKind::AssetPath;
        prop->Name = FName("StaticMeshAsset");
        prop->CPPType = "UStaticMesh*";
        prop->Offset = offsetof(UStaticMeshComponent, StaticMeshAsset);
        prop->ElementSize = sizeof(UStaticMesh*);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Asset";
        prop->DisplayName = "Static Mesh";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FStrProperty* prop = new FStrProperty();
        prop->Name = FName("StaticMeshAssetPath");
        prop->CPPType = "FString";
        prop->Offset = offsetof(UStaticMeshComponent, StaticMeshAssetPath);
        prop->ElementSize = sizeof(FString);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Asset";
        prop->DisplayName = "Static Mesh Path";
        info.ReflectedProperties.push_back(prop);
    }

    ReflectionDatabase::AddClass("UStaticMeshComponent", &info);
}

struct FAutoRegister_UStaticMeshComponent
{
    FAutoRegister_UStaticMeshComponent() { Register_UStaticMeshComponent(); }
};
static FAutoRegister_UStaticMeshComponent AutoRegister_UStaticMeshComponent_Instance;

void BindLua_UStaticMeshComponent(sol::state& Lua)
{
    sol::object UserTypeObject = Lua["StaticMeshComponent"];
    if (!UserTypeObject.valid() || UserTypeObject == sol::nil || UserTypeObject.get_type() != sol::type::table)
    {
        return;
    }
    sol::table UserType = UserTypeObject.as<sol::table>();

}

