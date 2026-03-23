#pragma once

#include "AActor.h"

class UTextRenderComponent;

class ACubeActor : public AActor
{
public:
	DECLARE_CLASS(ACubeActor, AActor)
	ACubeActor() = default;

	void InitDefaultComponents();
};

class ASphereActor : public AActor
{
public:
	DECLARE_CLASS(ASphereActor, AActor)
	ASphereActor() = default;

	void InitDefaultComponents();
};

class APlaneActor : public AActor
{
public:
	DECLARE_CLASS(APlaneActor, AActor)
	APlaneActor() = default;

	void InitDefaultComponents();
};

class AAttachTestActor : public AActor
{
public:
	DECLARE_CLASS(AAttachTestActor, AActor)
	AAttachTestActor() = default;

	void InitDefaultComponents();
};
