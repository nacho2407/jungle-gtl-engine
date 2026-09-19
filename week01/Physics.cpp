#include "Physics.hpp"

void ResolveCollision(UBall* A, UBall* B)
{
	FVector Delta = B->Location - A->Location;
	float Dist2 = Delta.Dot2(Delta);
	float r = A->Radius + B->Radius;

	if (Dist2 >= r * r)
	{
		return;
	}

	float Dist = sqrtf(Dist2);

	FVector Normal;
	if (Dist < 0.0001f)
	{
		float angle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
		Normal = FVector(cosf(angle), sinf(angle), 0.f);
	}
	else
	{
		float InverseDist = 1.0f / Dist;
		Normal = FVector(Delta.x * InverseDist, Delta.y * InverseDist, 0.0f);
	}


	// 안정성을 위해 공들이 겹치는 경우 처리
	float Penetration = r - Dist;
	float MA = A->Mass;
	float MB = B->Mass;

	const float Slop = 0.001f;
	const float Percent = 0.8f;
	float Correction = fmaxf(Penetration - Slop, 0.0f) / (1.0f / MA + 1.0f / MB) * Percent;
	float MoveA = Correction / MA;
	float MoveB = Correction / MB;

	A->Location = A->Location - (Normal * MoveA);
	B->Location = B->Location + (Normal * MoveB);

	A->BorderClamp();
	B->BorderClamp();

	// 관성 모멘트 
	float IA = 0.5f * MA * A->Radius * A->Radius;
	float IB = 0.5f * MB * B->Radius * B->Radius;
	float InvIA = 1.0f / IA, InvIB = 1.0f / IB;

	//접선의 방향
	FVector Tangent = FVector(-Normal.y, Normal.x, 0.f);

	// 충격량 적용 전 접촉점 속도 v + w corss r
	FVector vA_contact = FVector(
		A->Velocity.x - A->AngularVelocity * Normal.y * A->Radius,
		A->Velocity.y + A->AngularVelocity * Normal.x * A->Radius,
		0.f);

	FVector vB_contact = FVector(
		B->Velocity.x + B->AngularVelocity * Normal.y * B->Radius,
		B->Velocity.y - B->AngularVelocity * Normal.x * B->Radius,
		0.f);

	FVector RelVel = vB_contact - vA_contact;
	float RelVelNorm = RelVel.Dot2(Normal);

	// 이미 멀어지는 중이면 무시
	if (RelVelNorm > 0.0f)
		return;

	float RelVelTang = RelVel.Dot2(Tangent);

	const float e = 0.2f;
	float jN = -(1.0f + e) * RelVelNorm / (1 / MA + 1 / MB);

	float jT = 0.f;
	if (fabsf(RelVelTang) > 0.1f)
		jT = -RelVelTang / (1 / MA + 1 / MB + A->Radius * A->Radius * InvIA + B->Radius * B->Radius * InvIB);

	//마찰력 적용
	const float mu = 0.6f;
	float maxFriction = mu * fabsf(jN);
	if (fabsf(jT) > maxFriction)
		jT = (jT > 0.f ? 1.f : -1.f) * maxFriction;

	A->Velocity = A->Velocity - (Normal * (jN * 1 / MA)) - (Tangent * (jT * 1 / MA));
	B->Velocity = B->Velocity + (Normal * (jN * 1 / MB)) + (Tangent * (jT * 1 / MB));

	//충격량이 존재할 때만 각속도를 변화
	if (jN > 0.01f)
	{
		A->AngularVelocity -= A->Radius * jT * InvIA;
		B->AngularVelocity += B->Radius * jT * InvIB;
	}
}