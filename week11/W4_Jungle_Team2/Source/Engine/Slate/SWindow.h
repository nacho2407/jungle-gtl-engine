#pragma once
struct FRect
{
	float X = 0;
	float Y = 0;
	float Width = 0;
	float Height = 0;

	FRect() = default;
	FRect(float InX, float InY, float InWidth, float InHeight)
		:X(InX), Y(InY), Width(InWidth), Height(InHeight) { }
	FRect(float InX, float InY, float InWidthHeight)
		: FRect(InX, InY, InWidthHeight, InWidthHeight) { }
};

struct FPoint
{
	float X = 0; 
	float Y = 0;

	FPoint() = default;
	FPoint(float InX, float InY) : X(InX), Y(InY) { }
};

class SWindow
{
public:
	bool IsHovered(FPoint Coord) const;

	FRect GetRect() { return Rect; }
	const FRect& GetRect() const { return Rect; }

	void SetRect(FRect InRect) { Rect = InRect; }

private:
	FRect Rect;
};

