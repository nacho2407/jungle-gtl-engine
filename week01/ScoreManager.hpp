#pragma once

#include <cstdint>

// main이 너무 길어져서 일단 파일 분리 시켰습니다..

// 점수 관리 및 획득(감소는 없음)
class UScoreManager
{
public:
	UScoreManager() : Score(0) {}

	void AddScore(int score)
	{
		m_Score += score;
	}

	std::uint16_t GetScore() const
	{
		return Score;
	}
private:
	std::uint64_t Score;
	std::uint64
};