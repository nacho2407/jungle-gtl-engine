#include "FbxSceneImportContext.h"
#include "Core/Logging/Log.h"

#include <fbxsdk.h>
FFbxSceneImportContext::~FFbxSceneImportContext()
{
    Destroy();
}
bool FFbxSceneImportContext::Import(const FString& Path)
{
    Destroy();

    SourcePath = Path;

    Manager = fbxsdk::FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Failed to create FbxManager: %s", Path.c_str());
        return false;
    }
    fbxsdk::FbxIOSettings* IOSettings = fbxsdk::FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    Scene = fbxsdk::FbxScene::Create(Manager, "AnimImportScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Failed to create FbxScene: %s", Path.c_str());
        Destroy();
        return false;
    }

    fbxsdk::FbxImporter* Importer = fbxsdk::FbxImporter::Create(Manager, "");
    if (!Importer->Initialize(Path.c_str(), -1, Manager->GetIOSettings()))
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Initialize failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());

        Importer->Destroy();
        Destroy();
        return false;
    }

    const bool bImported = Importer->Import(Scene);
    if (!bImported)
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Import failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());

        Importer->Destroy();
        Destroy();
        return false;
    }

    Importer->Destroy();

    const fbxsdk::FbxAxisSystem TargetAxis(fbxsdk::FbxAxisSystem::eZAxis, fbxsdk::FbxAxisSystem::eParityOdd, fbxsdk::FbxAxisSystem::eLeftHanded);

    TargetAxis.DeepConvertScene(Scene);
    fbxsdk::FbxSystemUnit::m.ConvertScene(Scene);

    return true;
}

void FFbxSceneImportContext::Destroy()
{
    if (Manager)
    {
        Manager->Destroy();
        Manager = nullptr;
        Scene = nullptr;
    }
}
