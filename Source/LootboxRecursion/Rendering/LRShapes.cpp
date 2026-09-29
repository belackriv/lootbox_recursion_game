#include "Rendering/LRShapes.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

UStaticMesh* LRShapes::MakeTetrahedron(UObject* Outer)
{
	const FName SlotName(TEXT("Shape"));

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();

	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	UVs.SetNumChannels(1);

	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	SlotNames[Group] = SlotName;

	// Base corners at 90, 210 and 330 degrees, then the apex.
	const FVector3f Corners[4] = {
		FVector3f(0.f, 50.f, -50.f),
		FVector3f(-43.30127f, -25.f, -50.f),
		FVector3f(43.30127f, -25.f, -50.f),
		FVector3f(0.f, 0.f, 50.f),
	};
	FVertexID Vertices[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Vertices[Index] = Description.CreateVertex();
		Positions[Vertices[Index]] = Corners[Index];
	}

	// Each face is added in both windings, both with the outward normal. Whichever winding the
	// renderer treats as front-facing shows from outside, correctly lit, and the other is culled,
	// so the shape can't come out inside-out.
	const FVector3f Centre = (Corners[0] + Corners[1] + Corners[2] + Corners[3]) * 0.25f;
	const int32 Faces[4][3] = { { 0, 1, 2 }, { 0, 1, 3 }, { 1, 2, 3 }, { 2, 0, 3 } };
	const FVector2f FaceUVs[3] = { FVector2f(0.5f, 0.f), FVector2f(0.f, 1.f), FVector2f(1.f, 1.f) };
	for (const int32 (&Face)[3] : Faces)
	{
		const FVector3f A = Corners[Face[0]];
		const FVector3f B = Corners[Face[1]];
		const FVector3f C = Corners[Face[2]];
		FVector3f Normal = FVector3f::CrossProduct(B - A, C - A).GetSafeNormal();
		if (FVector3f::DotProduct(Normal, (A + B + C) / 3.f - Centre) < 0.f)
		{
			Normal = -Normal;
		}
		const FVector3f Tangent = (B - A).GetSafeNormal();

		for (const bool bReversed : { false, true })
		{
			TArray<FVertexInstanceID, TInlineAllocator<3>> Instances;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Pick = bReversed ? 2 - Corner : Corner;
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertices[Face[Pick]]);
				Normals[Instance] = Normal;
				Tangents[Instance] = Tangent;
				BinormalSigns[Instance] = 1.f;
				UVs.Set(Instance, 0, FaceUVs[Pick]);
				Instances.Add(Instance);
			}
			Description.CreateTriangle(Group, Instances);
		}
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, SlotName, SlotName));

	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bFastBuild = true;             // the runtime path: no editor-only build steps
	Params.bBuildSimpleCollision = true;  // a box, so the cursor trace can hit it
	Params.bCommitMeshDescription = false;
	Params.bMarkPackageDirty = false;
	Mesh->BuildFromMeshDescriptions({ &Description }, Params);
	return Mesh;
}
