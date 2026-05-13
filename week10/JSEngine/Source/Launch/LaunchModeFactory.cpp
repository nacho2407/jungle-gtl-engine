#include "Launch/LaunchModeFactory.h"

#include "Object/ObjectFactory.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif
#if IS_OBJ_VIEWER
#include "Misc/ObjViewer/ObjViewerEngine.h"
#endif
#if !WITH_EDITOR && !IS_OBJ_VIEWER
#include "Engine/Runtime/GameEngine.h"
#endif

UEngine* CreateLaunchEngine()
{
#if IS_OBJ_VIEWER
	return UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
	return UObjectManager::Get().CreateObject<UEditorEngine>();
#else
	return UObjectManager::Get().CreateObject<UGameEngine>();
#endif
}
