#pragma once

#include "Render/Execute/Context/Scene/SceneView.h"

class FScene;
class FViewModePassRegistry;
struct FCollectedPrimitives;

/*
    ���� �ܰ迡���� �ʿ��� ���� �����Դϴ�.
    �÷��Ͱ� ���������� ���� ���� ��ü�� �������� �ʵ���,
    ���� ��/��/���� ��å�� ���� ��� ����Ҹ� �����մϴ�.
*/
struct FRenderCollectContext
{
    const FSceneView* SceneView = nullptr;
    FScene* Scene = nullptr;

    const FViewModePassRegistry* ViewModePassRegistry = nullptr;
    EViewMode ActiveViewMode = {};

    FCollectedPrimitives* CollectedPrimitives = nullptr;
};
