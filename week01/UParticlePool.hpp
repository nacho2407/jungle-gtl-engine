#pragma once

#include "Containers.hpp"

#include "UParticle.hpp"
#include "URenderer.hpp"
#include "UTexture.hpp"

/*
 * 파티클 특성상 합쳐질때마다 생성을 할 경우 생명주기 관리가 어렵고
 * 생성되는데 비용도 고려하여 풀을 만듦
 */
class UParticlePool
{
public:
	UParticlePool() = default;
	~UParticlePool() = default;

public:
	/*머지를 위해 임시적으로 Texture를 Pool에 둠*/
	void Initialize(int DefaultCapacity, URenderer& renderer);

	void ReturnAll();

	void ReleasePool();

	void Update(float dt);

	void Render(URenderer& renderer);

	void Aquire(FVector Location);

	void Reset();

private:
	void Return(int idx);

private:
	TQueue<UParticle*> ParticlePool;
	TArray<UParticle*> ParticleList;
	UTexture* ParticleTexture;
};
