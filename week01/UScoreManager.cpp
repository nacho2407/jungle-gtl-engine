#include "UScoreManager.hpp"

void UScoreManager::AddScore(EFruitType FruitType)
{
	Score += (UINT)((FruitType * (FruitType + 1)) / 2);
}

void UScoreManager::Reset()
{
	Score = 0;
}

int UScoreManager::UpdateHighScore()
{
	for (int i = 0; i < RANK_NUM; i++)
	{
		if (Score > HighScores[i])
		{
			for (int j = RANK_NUM - 1; j > i; j--)
			{
				HighScores[j] = HighScores[j - 1];
			}

			HighScores[i] = Score;

			WriteHighScoreData();

			return i;
		}
	}
	return -1; // 랭킹 진입 실패
}

void UScoreManager::ReadHighScoreData()
{
	std::ifstream HighScoreData("highscore_data", std::ios::binary);

	if (!HighScoreData)
	{
		return;
	}

	HighScoreData.read(reinterpret_cast<char*>(HighScores), sizeof(HighScores));
}

void UScoreManager::WriteHighScoreData()
{
	std::ofstream HighScoreData("highscore_data", std::ios::binary); // 덮어쓰기

	HighScoreData.write(reinterpret_cast<const char*>(HighScores), sizeof(HighScores));
}
