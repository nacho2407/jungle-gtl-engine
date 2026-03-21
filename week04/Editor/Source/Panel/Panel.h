#pragma once

struct FEditorContext;

class IPanel
{
public:
	IPanel() = default;
	virtual ~IPanel() = default;

	virtual void Initialize(FEditorContext* InContext);
	virtual void ShutDown() {}

	virtual const wchar_t* GetPanelID() const = 0;
	virtual const wchar_t* GetDisplayName() const = 0;

	virtual void OnOpen() {}
	virtual void OnClose() {}
	virtual void Tick(float DeltaTime) {}
	virtual void Draw() = 0;

	bool IsOpen() const { return bOpen; }
	void SetOpen(bool bInOpen);
	void ToggleOpen();

protected:
	FEditorContext* GetContext() const { return Context; }

protected:
	FEditorContext* Context = nullptr;
private:
	bool bOpen = true;
};

