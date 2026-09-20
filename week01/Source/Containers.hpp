#pragma once

#include <windows.h>

// 구현 부분 분리하지 않고 헤더 전용 파일로 두었습니다.

template<typename DataType>
class TQueue
{
public:
	TQueue()
	{
		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
		Head = 0;
		Tail = 0;
	}

	~TQueue()
	{
		delete[] Data;
	}

	// Push, Pop, Reserve
	bool Push(const DataType& InData)
	{
		// Array is full
		if (DataNum == Capacity)
		{
			bool bIsReserveOK = Reserve(Capacity * 2);
			if (!bIsReserveOK) return false;
		}

		Data[Tail] = InData;
		Tail = (Tail + 1) % Capacity;
		DataNum++;
		return true;
	}

	void Clear()
	{
		delete[] Data;

		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
		Head = 0;
		Tail = 0;
	}

	void Pop()
	{
		if (IsEmpty()) return;
		Head = (Head + 1) % Capacity;
		DataNum--;
	}

	DataType& Front()
	{
		// Empty 체크 나중에 넣어야할듯
		return Data[Head];
	}

	const DataType& Front() const
	{
		// Empty 체크 나중에 넣어야할듯
		return Data[Head];
	}

	bool Reserve(UINT NewCapacity)
	{
		// 더 작아지는 경우는 고려하지 않음
		if (Capacity >= NewCapacity) return false;

		DataType* NewData = new DataType[NewCapacity];

		// 객체가 특수한 복사, 이동 연산을 가지는 것은 아직 고려하지 않음
		for (UINT i = 0; i < DataNum; i++)
		{
			NewData[i] = Data[(Head + i) % Capacity];
		}

		delete[] Data;
		Data = NewData;
		// DataNum 유지
		Capacity = NewCapacity;
		Head = 0;
		Tail = DataNum;
		return true;
	}

	bool IsEmpty() const
	{
		return DataNum == 0;
	}

	UINT Num() const
	{
		return DataNum;
	}

private:
	DataType* Data;
	UINT Head;
	UINT Tail;
	UINT DataNum;
	UINT Capacity;
};

template<typename DataType>
class TArray
{
public:
	TArray()
	{
		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
	}

	~TArray()
	{
		delete[] Data;
	}

	// PushBack, PopBack
	bool PushBack(const DataType& InData)
	{
		if (DataNum == Capacity)
		{
			bool bIsReserveOK = Reserve(Capacity * 2);
			if (!bIsReserveOK) return false;
		}

		Data[DataNum] = InData;
		DataNum++;
		return true;
	}

	void Clear()
	{
		delete[] Data;

		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
	}

	void PopBack()
	{
		if (IsEmpty()) return;

		DataNum--;
	}

	DataType& Back()
	{
		// Empty 처리 나중에 해야됨
		return Data[DataNum - 1];
	}

	DataType& operator[](UINT Idx)
	{
		// 일단 Out of index 는 체크 안함
		return Data[Idx];
	}

	const DataType& operator[](UINT Idx) const
	{
		return Data[Idx];
	}

	bool IsEmpty() const
	{
		return DataNum == 0;
	}

	UINT Num() const
	{
		return DataNum;
	}

	bool Reserve(UINT NewCapacity)
	{
		// 더 작아지는 경우는 허용 안함
		if (Capacity >= NewCapacity) return false;

		DataType* NewData = new DataType[NewCapacity];
		for (UINT i = 0; i < DataNum; i++)
		{
			NewData[i] = Data[i];
		}

		delete[] Data;
		Data = NewData;
		// DataNum 유지
		Capacity = NewCapacity;
		return true;
	}

	void RemoveAt(UINT Idx)
	{
		if (Idx >= DataNum)
		{
			return;
		}

		if (Idx != DataNum - 1)
		{
			Data[Idx] = Data[DataNum - 1];
		}

		DataNum--;

		// 원소의 소멸자를 별도로 호출하지 않음에 유의
	}

private:
	DataType* Data;
	UINT DataNum;
	UINT Capacity;
};
