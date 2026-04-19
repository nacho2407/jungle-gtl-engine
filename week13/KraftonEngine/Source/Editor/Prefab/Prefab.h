#pragma once
#include "Render/Types/RenderTypes.h"
#include "GameFramework/AActor.h"

class AActor;

class FPrefab
{
public:
	virtual ~FPrefab();

	AActor* GetActor() const { return Actor; }
	void SetActor(AActor* InActor) { Actor = InActor; }
	AActor* GetActorDup() { return static_cast<AActor*>(Actor->Duplicate()); }

	ID3D11Texture2D* GetImage() const { return ActorImage; }
	void SetImage(ID3D11Texture2D* InImage) { ActorImage = InImage; }

private:
	AActor* Actor;
	ID3D11Texture2D* ActorImage;
};

