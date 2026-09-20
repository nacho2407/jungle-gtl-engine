#pragma once

#include <fstream>

#include <windows.h>

#include "EFruitType.h"

class UScoreManager
{
public:
	UScoreManager() : Score(0)
	{
		ReadHighScoreData();
	}

	UINT GetScore() const
	{
		return Score;
	}

	UINT* GetHighScores() {
		return HighScores;
	}

	/**
	 * @param FruitType 병합된 후의 과일 타입
	 */
	void AddScore(EFruitType FruitType);

	void Reset();

	int UpdateHighScore();

private:
	void ReadHighScoreData();

	void WriteHighScoreData();

private:
	UINT Score;
	UINT HighScores[5] = { 0 }; // 0:최고기록
};

const UINT RANK_NUM = 5;
