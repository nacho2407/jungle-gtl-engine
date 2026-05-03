#include "ActorComponent.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cctype>
#include <cstring>

DEFINE_CLASS(UActorComponent, UObject)
REGISTER_FACTORY(UActorComponent)

namespace
{
    FString TrimComponentTag(const FString& Value)
    {
        const auto IsSpace = [](unsigned char Ch)
        {
            return std::isspace(Ch) != 0;
        };

        auto Begin = std::find_if_not(Value.begin(), Value.end(), IsSpace);
        auto End = std::find_if_not(Value.rbegin(), Value.rend(), IsSpace).base();
        if (Begin >= End)
        {
            return {};
        }
        return FString(Begin, End);
    }
}

void UActorComponent::BeginPlay()
{
    if (bAutoActivate)
    {
        Activate();
    }
}

void UActorComponent::Activate()
{
    bCanEverTick = true;
}

void UActorComponent::Deactivate()
{
    bCanEverTick = false;
}

void UActorComponent::ExecuteTick(float DeltaTime)
{
    if (bCanEverTick == false || bIsActive == false)
    {
        return;
    }

    TickComponent(DeltaTime);
}

void UActorComponent::SetActive(bool bNewActive)
{
    if (bNewActive == bIsActive)
    {
        return;
    }

    bIsActive = bNewActive;

    if (bIsActive)
    {
        Activate();
    }
    else
    {
        Deactivate();
    }
}

void UActorComponent::AddTag(const FString& Tag)
{
    const FString CleanTag = TrimComponentTag(Tag);
    if (CleanTag.empty() || HasTag(CleanTag))
    {
        return;
    }

    Tags.push_back(CleanTag);
}

void UActorComponent::RemoveTag(const FString& Tag)
{
    const FString CleanTag = TrimComponentTag(Tag);
    Tags.erase(
        std::remove(Tags.begin(), Tags.end(), CleanTag),
        Tags.end());
}

bool UActorComponent::HasTag(const FString& Tag) const
{
    const FString CleanTag = TrimComponentTag(Tag);
    return std::find(Tags.begin(), Tags.end(), CleanTag) != Tags.end();
}

void UActorComponent::ClearTags()
{
    Tags.clear();
}

FString UActorComponent::GetTagsText() const
{
    FString Result;
    for (size_t Index = 0; Index < Tags.size(); ++Index)
    {
        if (Index > 0)
        {
            Result += ", ";
        }
        Result += Tags[Index];
    }
    return Result;
}

void UActorComponent::SetTagsFromText(const FString& InTagsText)
{
    Tags.clear();

    size_t Start = 0;
    while (Start <= InTagsText.size())
    {
        const size_t Comma = InTagsText.find(',', Start);
        const size_t End = (Comma == FString::npos) ? InTagsText.size() : Comma;
        AddTag(InTagsText.substr(Start, End - Start));

        if (Comma == FString::npos)
        {
            break;
        }
        Start = Comma + 1;
    }
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    TagsText = GetTagsText();
    // OutProps.push_back({"Active", EPropertyType::Bool, &bIsActive});
    // OutProps.push_back({"Auto Activate", EPropertyType::Bool, &bAutoActivate});
    OutProps.push_back({"Enable Tick", EPropertyType::Bool, &bCanEverTick});
    OutProps.push_back({"Be Serialized", EPropertyType::Bool, &bTransient});
    OutProps.push_back({"Editor Only", EPropertyType::Bool, &bIsEditorOnly});
    OutProps.push_back({"Tags", EPropertyType::String, &TagsText});
}

void UActorComponent::PostEditProperty(const char* PropertyName)
{
    if (PropertyName && std::strcmp(PropertyName, "Tags") == 0)
    {
        SetTagsFromText(TagsText);
    }
}

void UActorComponent::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar << "Enable Tick" << bCanEverTick;
	Ar << "Editor Only" << bIsEditorOnly;
	Ar << "Tags" << Tags;
}
