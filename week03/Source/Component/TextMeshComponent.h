#pragma once

#include "PrimitiveComponent.h"
#include "Renderer/Geometry.h"
#include "VertexSimple.h"

class UTextMeshComponent : public UPrimitiveComponent
{
	DECLARE_RTTI(UTextMeshComponent, UPrimitiveComponent)
public:
	UTextMeshComponent();
	virtual ~UTextMeshComponent() override = default;

	virtual void OnComponentAdded() override;
	virtual void Update(float DeltaTime) override;
	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON JsonObj) override;

	void SetText(const FString& InText);
	const FString& GetText() const { return Text; }

	const TArray<FVertexSimple>& GetTextVertices() const { return TextVertices; }

private:
	virtual RenderObject* CreateRenderObject() override;
	virtual void UpdateRenderObject() override;

	void RebuildTextGeometry();

private:
	FString Text = "TextMesh";
	FColor TextColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);
	FGeometry TextGeometry;
	TArray<FVertexSimple> TextVertices;
};
