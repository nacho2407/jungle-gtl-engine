#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

class UParticleSystem;

class FParticleSystemEditorWidget : public FAssetEditorWidget
{
public:
    FParticleSystemEditorWidget()           = default;
    ~FParticleSystemEditorWidget() override = default;

    bool CanEdit(UObject* Object) const override;
    bool IsEditingObject(UObject* Object) const override;

    void Open(UObject* Object) override;
    void Close() override;
    void Tick(float DeltaTime) override;
    void Render(float DeltaTime) override;

    bool AllowsMultipleInstances() const override { return true; }

private:
    UParticleSystem* GetParticleSystem() const;

private:
    FString WindowTitle = "Particle System Editor";
};
