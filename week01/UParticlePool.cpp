#include "UParticlePool.hpp"

void UParticlePool::Initialize(int DefaultCapacity, URenderer& renderer)
{
	ParticleTexture = new UTexture;
	ParticleTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/Star.png");

	ParticlePool.Reserve(DefaultCapacity);
	for (int i = 0; i < DefaultCapacity; ++i)
	{
		UParticle* particle = new UParticle();
		particle->SetActive(false, 0);
		particle->SetTexture(ParticleTexture);
		ParticlePool.Push(particle);
	}
}
void UParticlePool::ReturnAll()
{
	for (UINT i = 0; i < ParticleList.Num(); ++i)
	{
		//강제로 풀에 반납
		ParticleList[i]->SetActive(true, 0);
		Return(i);
	}
}
void UParticlePool::ReleasePool()
{
	while (!ParticlePool.IsEmpty())
	{
		UParticle* particle = ParticlePool.Front();
		ParticlePool.Pop();
		delete particle;
	}
	for (int i = 0; i < ParticleList.Num(); ++i)
	{
		delete ParticleList[i];
		ParticleList[i] = nullptr;
	}
	if (ParticleTexture)
	{
		delete ParticleTexture;
		ParticleTexture = nullptr;
	}
}

void UParticlePool::Update(float dt)
{
	for (UINT i = 0; i < ParticleList.Num(); ++i)
	{
		ParticleList[i]->UpdatePhysics(dt);
	}

	for (UINT i = 0; i < ParticleList.Num(); ++i)
	{
		if (!ParticleList[i]->IsActive())
		{
			Return(i);
		}
	}
}

void UParticlePool::Render(URenderer& renderer)
{
	for (int i = 0; i < ParticleList.Num(); ++i)
	{
		ParticleList[i]->Render(renderer);

	}
}

void UParticlePool::Aquire(FVector Location)
{
	//소유권 이동
	if (ParticlePool.IsEmpty()) return;

	UINT pairticleNum = 5;
	if (pairticleNum % 2 == 0) ++pairticleNum;

	for (UINT i = 1; i <= pairticleNum; ++i)
	{
		UParticle* particle = ParticlePool.Front();
		particle->SetLocation(Location);
		particle->SetActive(true, 360 * i / pairticleNum);

		//각도 설정
		ParticlePool.Pop();
		ParticleList.PushBack(particle);
	}
}

void UParticlePool::Reset()
{
	for (UINT i = 0; i < ParticleList.Num(); ++i)
	{
		if (ParticleList[i])
		{
			ParticleList[i]->SetActive(false, 0);
			ParticlePool.Push(ParticleList[i]);
			ParticleList[i] = nullptr;
		}
	}

	ParticleList.Clear();
}

void UParticlePool::Return(int idx)
{
	ParticleList[idx]->SetActive(false, 0);

	ParticlePool.Push(ParticleList[idx]);

	ParticleList.RemoveAt(idx);
}
