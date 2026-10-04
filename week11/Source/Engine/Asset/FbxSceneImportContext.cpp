#include "FbxSceneImportContext.h"
#include "Core/Logging/Log.h"

#include <fbxsdk.h>

namespace
{
    class FScopedFbxImporter
    {
    public:
        explicit FScopedFbxImporter(fbxsdk::FbxImporter* InImporter)
            : Importer(InImporter)
        {
        }

        ~FScopedFbxImporter()
        {
            Reset();
        }

        FScopedFbxImporter(const FScopedFbxImporter&) = delete;
        FScopedFbxImporter& operator=(const FScopedFbxImporter&) = delete;

        fbxsdk::FbxImporter* Get() const
        {
            return Importer;
        }

        fbxsdk::FbxImporter* operator->() const
        {
            return Importer;
        }

        void Reset()
        {
            if (Importer)
            {
                Importer->Destroy();
                Importer = nullptr;
            }
        }

    private:
        fbxsdk::FbxImporter* Importer = nullptr;
    };
}

FFbxSceneImportContext::~FFbxSceneImportContext()
{
    Destroy();
}
bool FFbxSceneImportContext::Import(const FString& Path, const FString& AnimStackName)
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

    FScopedFbxImporter Importer(fbxsdk::FbxImporter::Create(Manager, ""));
    if (!Importer.Get())
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Failed to create FbxImporter: %s", Path.c_str());
        Destroy();
        return false;
    }

    if (!Importer->Initialize(Path.c_str(), -1, Manager->GetIOSettings()))
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Initialize failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());

        Destroy();
        return false;
    }

    if (!AnimStackName.empty())
    {
        bool bFoundRequestedTake = false;
        const int32 StackCount = Importer->GetAnimStackCount();
        for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
        {
            fbxsdk::FbxTakeInfo* TakeInfo = Importer->GetTakeInfo(StackIndex);
            if (!TakeInfo)
            {
                continue;
            }

            const FString TakeName(TakeInfo->mName.Buffer());
            const FString ImportName(TakeInfo->mImportName.Buffer());
            const bool bMatches = TakeName == AnimStackName || ImportName == AnimStackName;
            TakeInfo->mSelect = bMatches;

            if (bMatches)
            {
                TakeInfo->mImportName = AnimStackName.c_str();
                bFoundRequestedTake = true;
            }
        }

        bRequestedAnimStackFound = bFoundRequestedTake;
        Manager->GetIOSettings()->SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, AnimStackName.c_str());

        if (!bFoundRequestedTake)
        {
            UE_LOG_WARNING("[FbxSceneImportContext] Requested anim stack was not found in take info. Fbx=%s Stack=%s",
                           Path.c_str(),
                           AnimStackName.c_str());
        }
    }

    const bool bImported = Importer->Import(Scene);
    if (!bImported)
    {
        UE_LOG_ERROR("[FbxSceneImportContext] Import failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());

        Destroy();
        return false;
    }

    Importer.Reset();

    const fbxsdk::FbxAxisSystem TargetAxis(fbxsdk::FbxAxisSystem::eZAxis, fbxsdk::FbxAxisSystem::eParityOdd, fbxsdk::FbxAxisSystem::eLeftHanded);

    TargetAxis.DeepConvertScene(Scene);
    fbxsdk::FbxSystemUnit::m.ConvertScene(Scene);

    return true;
}

void FFbxSceneImportContext::Destroy()
{
    bRequestedAnimStackFound = false;

    if (Manager)
    {
        Manager->Destroy();
        Manager = nullptr;
        Scene = nullptr;
    }
}
