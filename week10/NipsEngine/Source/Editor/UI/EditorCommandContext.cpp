#include "Editor/UI/EditorCommandContext.h"

bool FEditorShortcut::IsValid() const
{
	return Key != 0;
}

bool FEditorShortcut::Matches(const FEditorShortcut& Other) const
{
	return Key == Other.Key
		&& bCtrl == Other.bCtrl
		&& bShift == Other.bShift
		&& bAlt == Other.bAlt;
}

void FEditorCommandList::Clear()
{
	Bindings.clear();
}

void FEditorCommandList::MapAction(
	EEditorCommandId CommandId,
	const FEditorShortcut& Shortcut,
	std::function<void()> Execute,
	std::function<bool()> CanExecute)
{
	FEditorCommandBinding Binding;
	Binding.CommandId = CommandId;
	Binding.Shortcut = Shortcut;
	Binding.Execute = std::move(Execute);
	Binding.CanExecute = std::move(CanExecute);

	Bindings.emplace_back(std::move(Binding));
}

bool FEditorCommandList::TryExecuteShortcut(const FEditorShortcut& Shortcut) const
{
	if (!Shortcut.IsValid())
	{
		return false;
	}

	for (const FEditorCommandBinding& Binding : Bindings)
	{
		if (!Binding.Shortcut.Matches(Shortcut))
		{
			continue;
		}

		if (Binding.CanExecute && !Binding.CanExecute())
		{
			return false;
		}

		if (Binding.Execute)
		{
			Binding.Execute();
			return true;
		}
	}

	return false;
}

bool FEditorCommandList::TryExecuteCommand(EEditorCommandId CommandId) const
{
	for (const FEditorCommandBinding& Binding : Bindings)
	{
		if (Binding.CommandId != CommandId)
		{
			continue;
		}

		if (Binding.CanExecute && !Binding.CanExecute())
		{
			return false;
		}

		if (Binding.Execute)
		{
			Binding.Execute();
			return true;
		}
	}

	return false;
}

bool FEditorCommandList::CanExecute(EEditorCommandId CommandId) const
{
	for (const FEditorCommandBinding& Binding : Bindings)
	{
		if (Binding.CommandId == CommandId)
		{
			return !Binding.CanExecute || Binding.CanExecute();
		}
	}

	return false;
}
