#include "../Public/VoxelModuleEditor.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Async/ParallelFor.h"
#include "SkeletalMeshAttributes.h"
#include "MeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "../../VoxelModule/Public/UVoxelBodyAsset.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelEditor, Log, All);

IMPLEMENT_MODULE(FVoxelModuleEditor, VoxelModuleEditor);

void FVoxelModuleEditor::StartupModule()
{
	UE_LOG(LogVoxelEditor, Log, TEXT("FVoxelModuleEditor charge"));
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVoxelModuleEditor::RegisterMenus));
}

void FVoxelModuleEditor::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}


void FVoxelModuleEditor::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SkeletalMesh");
	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetSections");
	FToolUIAction Action;
	Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context)
		{
			UE_LOG(LogVoxelEditor, Warning, TEXT("Voxeliser : pas de contexte Content Browser"));
			return;
		}
		for (USkeletalMesh* Mesh : Context->LoadSelectedObjects<USkeletalMesh>())
		{
			if (!Mesh)
			{
				continue;
			}
			const FVector Size = Mesh->GetBounds().GetBox().GetSize();
			const USkeleton* Skeleton = Mesh->GetSkeleton();
			UE_LOG(LogVoxelEditor, Log, TEXT("Voxeliser : %s | Taille (cm) = %s | LODs = %d | Os = %d | Squelette = %s"),
				*Mesh->GetName(),
				*Size.ToString(),
				Mesh->GetLODNum(),
				Mesh->GetRefSkeleton().GetNum(),
				Skeleton ? *Skeleton->GetName() : TEXT("aucun"));

			FBindPoseGeometry MeshGeometry;
			if (!ExtractBindPoseGeometry(*Mesh, 0, MeshGeometry))
			{
				continue;
			}
			UE_LOG(LogVoxelEditor, Log, TEXT("Geometrie : %d sommets | %d triangles | Bornes exactes (cm) = %s"),
				MeshGeometry.Positions.Num(),
				MeshGeometry.Triangles.Num(),
				*MeshGeometry.Bounds.GetSize().ToString());

			//Log des os pour tester
			int32 TopVertex = 0;
			int32 BottomVertex = 0;
			int32 SideVertex = 0;

			for (int32 V = 0; V < MeshGeometry.Positions.Num(); V++)
			{
				const FVector3f& Position = MeshGeometry.Positions[V];
				if (Position.Z > MeshGeometry.Positions[TopVertex].Z) { TopVertex = V; }
				if (Position.Z < MeshGeometry.Positions[BottomVertex].Z) { BottomVertex = V; }
				if (Position.X > MeshGeometry.Positions[SideVertex].X) { SideVertex = V; }
			}

			const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
			auto LogVertexBone = [&MeshGeometry, &RefSkeleton](const TCHAR* Label, const int32 V)
				{
					const int32 BoneIndex = MeshGeometry.DominantBone[V];
					const FString BoneName = RefSkeleton.IsValidIndex(BoneIndex)
						? RefSkeleton.GetBoneName(BoneIndex).ToString()
						: TEXT("INDEX INVALIDE");
					UE_LOG(LogVoxelEditor, Log, TEXT("5a | %s : sommet %d %s -> os %d (%s)"),
						Label, V, *MeshGeometry.Positions[V].ToString(), BoneIndex, *BoneName);
				};
			LogVertexBone(TEXT("Plus haut "), TopVertex);
			LogVertexBone(TEXT("Plus bas  "), BottomVertex);
			LogVertexBone(TEXT("Plus a X+ "), SideVertex);

			const FVector3d BoundsMin = FVector3d(MeshGeometry.Bounds.Min);
			const FVector3d BoundsSize = FVector3d(MeshGeometry.Bounds.GetSize());
			const FVector3d BoundsCenter = FVector3d(MeshGeometry.Bounds.GetCenter());


			const double PX = BoundsCenter.X; //Millieu du corps
			const double PZ = BoundsMin.Z + 0.72 * BoundsSize.Z; //Hauteur poitrine
			const float CellSize = 2.5f;
			const int32  Padding = 2;
			const int32 NumSteps = FMath::CeilToInt(BoundsSize.Y / CellSize) + 2 * Padding;

			for (int32 i = 0; i < NumSteps; i++)
			{
				const double Y = BoundsMin.Y - Padding * CellSize + (i + 0.5) * CellSize;
				const FVector3d P(PX, Y, PZ);
				const double W = ComputeWindingNumber(MeshGeometry, P);
				UE_LOG(LogVoxelEditor, Log, TEXT("Profil [%2d] : Y = %7.2f cm | w = %6.3f"), i, Y, W);
			}
			
			FVoxelGrid VoxelGrid;
			BuildVoxelGrid(MeshGeometry, CellSize, Padding, true, VoxelGrid);
			AssignVoxelBones(MeshGeometry, VoxelGrid);
			LogFrontBones(VoxelGrid, RefSkeleton);
			CreateVoxelBodyAsset(*Mesh, VoxelGrid);

		}
	});
	Section.AddMenuEntry(
		"VoxelizeSkeletalMesh",
		FText::FromString(TEXT("Voxeliser")),
		FText::FromString(TEXT("Voxelise ce skeletal mesh en pose de repos")),
		FSlateIcon(),
		Action);

}

bool FVoxelModuleEditor::ExtractBindPoseGeometry(const USkeletalMesh& Mesh, const int32 LODIndex, FBindPoseGeometry& Out)
{
	Out = FBindPoseGeometry();
	int32 BoneNull = 0;
	FMeshDescription MeshDescription;
	if (!Mesh.CloneMeshDescription(LODIndex, MeshDescription))
	{
		UE_LOG(LogVoxelEditor, Error, TEXT("static bool ExtractBindPoseGeometry(const USkeletalMesh& Mesh, const int32 LODIndex, FBindPoseGeometry& Out) -> ExtractBindPoseGeometry : pas de MeshDescription pour le LOD %d"), LODIndex);
		return false;
	}

	TVertexAttributesConstRef<FVector3f> VertexPositions = MeshDescription.GetVertexPositions();
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, MeshDescription.Vertices().GetArraySize());
	Out.Positions.Reserve(MeshDescription.Vertices().Num());
	FSkeletalMeshConstAttributes Attributes(MeshDescription);
	FSkinWeightsVertexAttributesConstRef SkinWeight = Attributes.GetVertexSkinWeights();
	Out.DominantBone.Reserve(MeshDescription.Vertices().Num());
	for (const FVertexID VertexId : MeshDescription.Vertices().GetElementIDs())
	{
		const FVector3f VertexPosition = VertexPositions[VertexId];
		Remap[VertexId.GetValue()] = Out.Positions.Add(VertexPosition);
		Out.Bounds += VertexPosition;
		
		int32 BestBone = INDEX_NONE;
		float BestWeight = -1.0f;
		for (const UE::AnimationCore::FBoneWeight& BoneWeight : SkinWeight.Get(VertexId))
		{
			if (BoneWeight.GetWeight() > BestWeight)
			{
				BestWeight = BoneWeight.GetWeight();
				BestBone = BoneWeight.GetBoneIndex();
			}
		}
		Out.DominantBone.Add(BestBone);
		
		if (BestBone == INDEX_NONE)
		{
			BoneNull++;
		}

	}
	UE_LOG(LogVoxelEditor, Log, TEXT("FVoxelModuleEditor::ExtractBindPoseGeometry --> BoneNull = %d"), BoneNull);
	

	Out.Triangles.Reserve(MeshDescription.Triangles().Num());
	for (const FTriangleID TriangleID : MeshDescription.Triangles().GetElementIDs())
	{
		TArrayView<const FVertexID> VerticesId = MeshDescription.GetTriangleVertices(TriangleID);
		Out.Triangles.Add(FIntVector3(Remap[VerticesId[0].GetValue()], Remap[VerticesId[1].GetValue()], Remap[VerticesId[2].GetValue()]));;
	}
	return Out.Triangles.Num() > 0;
}

void FVoxelModuleEditor::BuildVoxelGrid(const FBindPoseGeometry& Geometry, const double CellSize, const int32 Padding, const bool bUseParallel, FVoxelGrid& Out)
{
	Out = FVoxelGrid();
	const FVector3d BoundsMin = FVector3d(Geometry.Bounds.Min);
	const FVector3d BoundsSize = FVector3d(Geometry.Bounds.GetSize());
	Out.CellSize = CellSize;
	Out.Origin = BoundsMin - FVector3d(Padding * CellSize);

	Out.Dims = FIntVector(
		FMath::CeilToInt(BoundsSize.X / CellSize) + 2 * Padding,
		FMath::CeilToInt(BoundsSize.Y / CellSize) + 2 * Padding,
		FMath::CeilToInt(BoundsSize.Z / CellSize) + 2 * Padding);

	int32 NumCells = Out.Dims.X * Out.Dims.Y * Out.Dims.Z;
	Out.Winding.SetNumZeroed(NumCells);
	UE_LOG(LogVoxelEditor, Log, TEXT("BuildVoxelGrid : %d x %d x %d = %d cellules"),
		Out.Dims.X, Out.Dims.Y, Out.Dims.Z, NumCells);

	auto ComputeSlice = [&Geometry, &Out](const int32 K)
		{
			for (int32 J = 0; J < Out.Dims.Y; J++)
				for (int32 I = 0; I < Out.Dims.X; I++)
				{
					const FVector3d P = Out.CellCenter(I, J, K);

					Out.Winding[Out.Index(I, J, K)] = static_cast<float>(ComputeWindingNumber(Geometry, P));
				}
		};
	const double StartTime = FPlatformTime::Seconds();
	if (bUseParallel)
	{
		ParallelFor(Out.Dims.Z, ComputeSlice);
	}
	else
	{
		for (int32 K = 0; K < Out.Dims.Z; K++)
		{
			ComputeSlice(K);
		}
	}
	UE_LOG(LogVoxelEditor, Log, TEXT("BuildVoxelGrid (%s) : calculee en %.2f s"),
		bUseParallel ? TEXT("parallele") : TEXT("sequentiel"),
		FPlatformTime::Seconds() - StartTime);
}



double FVoxelModuleEditor::ComputeWindingNumber(const FBindPoseGeometry& Geometry, const FVector3d& P)
{
	double SumOmega = 0.0;
	
	for (const FIntVector3& Triangle : Geometry.Triangles)
	{
		//A B et C réprésentent les coins des traits qui constituent un triangle    V0  
		const FVector3d V0 = FVector3d(Geometry.Positions[Triangle.X]);    //       / \    
		const FVector3d V1 = FVector3d(Geometry.Positions[Triangle.Y]);    //      /   \ 
		const FVector3d V2 = FVector3d(Geometry.Positions[Triangle.Z]);    //   V1/_____\V2

		//P représente mes yeux et A, B et C sont les droites qui partent de P mes yeux vers V0, V1 et V2
		const FVector3d A = V0 - P; 
		const FVector3d B = V1 - P;
		const FVector3d C = V2 - P;

		//Longueur dses droites
		const double LenA = A.Length();
		const double LenB = B.Length();
		const double LenC = C.Length();

		const double Numerator = FVector3d::DotProduct(A, FVector3d::CrossProduct(B, C)); // Positif P est devant Nagatif P est derrière

		const double Denominator = LenA * LenB * LenC
			+ FVector3d::DotProduct(A, B) * LenC
			+ FVector3d::DotProduct(A, C) * LenB
			+ FVector3d::DotProduct(B, C) * LenA;

		SumOmega += 2.0 * FMath::Atan2(Numerator, Denominator);
	}
	return SumOmega / (4 * UE_DOUBLE_PI);
}

void FVoxelModuleEditor::LogSliceAscii(const FBindPoseGeometry& Geometry, const double HeightRatio)
{
	const double CellSize = 2.5;
	const int32  Padding = 2;

	const FVector3d BoundsMin = FVector3d(Geometry.Bounds.Min);
	const FVector3d BoundsSize = FVector3d(Geometry.Bounds.GetSize());

	const double TestZ = BoundsMin.Z + HeightRatio * BoundsSize.Z;

	const int32 NumX = FMath::CeilToInt(BoundsSize.X / CellSize) + 2 * Padding;
	const int32 NumY = FMath::CeilToInt(BoundsSize.Y / CellSize) + 2 * Padding;

	const double StartTime = FPlatformTime::Seconds();

	UE_LOG(LogVoxelEditor, Log, TEXT("Coupe a %.0f%% de la hauteur (Z = %.2f cm) :"), HeightRatio * 100.0, TestZ);

	for (int32 j = 0; j < NumY; ++j)
	{
		const double Y = BoundsMin.Y - Padding * CellSize + (j + 0.5) * CellSize;

		FString Line;
		Line.Reserve(NumX);

		for (int32 i = 0; i < NumX; ++i)
		{
			const double X = BoundsMin.X - Padding * CellSize + (i + 0.5) * CellSize;

			const double W = FMath::Abs(ComputeWindingNumber(Geometry, FVector3d(X, Y, TestZ)));

			TCHAR Cell = TEXT('.');
			if (W >= 1.5) { Cell = TEXT('2'); }
			else if (W >= 0.5) { Cell = TEXT('#'); }

			Line.AppendChar(Cell);
		}

		UE_LOG(LogVoxelEditor, Log, TEXT("%s"), *Line);
	}

	UE_LOG(LogVoxelEditor, Log, TEXT("Coupe calculee en %.2f s"), FPlatformTime::Seconds() - StartTime);
}

void FVoxelModuleEditor::LogFrontSilhouette(const FVoxelGrid& Grid)
{
	UE_LOG(LogVoxelEditor, Log, TEXT("Silhouette de face (%d x %d) :"), Grid.Dims.X, Grid.Dims.Z);

	// K décroissant : on commence par le HAUT de la grille, sinon le personnage s'affiche la tête en bas
	for (int32 K = Grid.Dims.Z - 1; K >= 0; --K)
	{
		FString Line;
		Line.Reserve(Grid.Dims.X);

		for (int32 I = 0; I < Grid.Dims.X; ++I)
		{
			// Y a-t-il au moins une cellule pleine sur toute la profondeur de cette colonne ?
			bool bAnyFilled = false;
			for (int32 J = 0; J < Grid.Dims.Y; ++J)
			{
				if (FMath::Abs(Grid.Winding[Grid.Index(I, J, K)]) > 0.5f)
				{
					bAnyFilled = true;
					break;   // une seule suffit : inutile de regarder le reste de la colonne
				}
			}

			Line.AppendChar(bAnyFilled ? TEXT('#') : TEXT('.'));
		}

		UE_LOG(LogVoxelEditor, Log, TEXT("%s"), *Line);
	}
}

void FVoxelModuleEditor::AssignVoxelBones(const FBindPoseGeometry& Geometry, FVoxelGrid& Grid)
{
	Grid.Bone.Init(static_cast<int16>(INDEX_NONE), Grid.Winding.Num());
	const double StartTime = FPlatformTime::Seconds();

	ParallelFor(Grid.Dims.Z, [&Geometry, &Grid](const int32 K)
		{
			for(int32 J = 0; J < Grid.Dims.Y; J++)
				for (int32 I = 0; I < Grid.Dims.X; I++)
				{
					const int32 CellIndex = Grid.Index(I, J, K);

					if (FMath::Abs(Grid.Winding[CellIndex]) <= 0.5f)
					{
						continue;
					}
					const FVector3d P = Grid.CellCenter(I, J, K);
					int32 BestVertex = INDEX_NONE;
					double BestDistSq = TNumericLimits<double>::Max();

					for (int32 V = 0; V < Geometry.Positions.Num(); ++V)
					{
						const double DistSq = FVector3d::DistSquared(P, FVector3d(Geometry.Positions[V]));
						if (DistSq < BestDistSq)
						{
							BestDistSq = DistSq;
							BestVertex = V;
						}
					}
					Grid.Bone[CellIndex] = static_cast<int16>(Geometry.DominantBone[BestVertex]);
				}
		});
	int32 NumFilled = 0;
	for (const int16 BoneIndex : Grid.Bone)
	{
		if (BoneIndex != INDEX_NONE)
		{
			++NumFilled;
		}
	}

	UE_LOG(LogVoxelEditor, Log, TEXT("AssignVoxelBones : %d cellules pleines sur %d, calcule en %.2f s"),
		NumFilled, Grid.Bone.Num(), FPlatformTime::Seconds() - StartTime);
}

void FVoxelModuleEditor::LogFrontBones(const FVoxelGrid& Grid, const FReferenceSkeleton& RefSkeleton)
{
	// 62 symboles pour 89 os : deux os peuvent partager un symbole (os 0 et os 62 ? '0').
	// Acceptable pour un contrôle visuel ; la légende lève l'ambiguïté.
	static const TCHAR* Symbols = TEXT("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	const int32 NumSymbols = FCString::Strlen(Symbols);

	// Mémorise quels os apparaissent réellement, pour n'afficher qu'eux dans la légende
	TArray<bool> bBoneUsed;
	bBoneUsed.Init(false, RefSkeleton.GetNum());

	UE_LOG(LogVoxelEditor, Log, TEXT("Silhouette des os, vue de face (%d x %d) :"), Grid.Dims.X, Grid.Dims.Z);

	for (int32 K = Grid.Dims.Z - 1; K >= 0; --K)   // du haut vers le bas
	{
		FString Line;
		Line.Reserve(Grid.Dims.X);

		for (int32 I = 0; I < Grid.Dims.X; ++I)
		{
			TCHAR Cell = TEXT('.');

			// Première cellule pleine en partant de l'avant (J = 0) : c'est celle qu'on « voit »
			for (int32 J = 0; J < Grid.Dims.Y; ++J)
			{
				const int16 Bone = Grid.Bone[Grid.Index(I, J, K)];
				if (Bone != INDEX_NONE)
				{
					Cell = Symbols[Bone % NumSymbols];
					if (bBoneUsed.IsValidIndex(Bone))
					{
						bBoneUsed[Bone] = true;
					}
					break;
				}
			}

			Line.AppendChar(Cell);
		}

		UE_LOG(LogVoxelEditor, Log, TEXT("%s"), *Line);
	}

	UE_LOG(LogVoxelEditor, Log, TEXT("Legende :"));
	for (int32 BoneIndex = 0; BoneIndex < bBoneUsed.Num(); ++BoneIndex)
	{
		if (bBoneUsed[BoneIndex])
		{
			UE_LOG(LogVoxelEditor, Log, TEXT("  %c = %s (%d)"),
				Symbols[BoneIndex % NumSymbols],
				*RefSkeleton.GetBoneName(BoneIndex).ToString(),
				BoneIndex);
		}
	}
}

UVoxelBodyAsset* FVoxelModuleEditor::CreateVoxelBodyAsset(USkeletalMesh& SourceMesh, const FVoxelGrid& Grid)
{
	// ---------- 1. Contrôles préalables ----------
	// Les coordonnées sont stockées en int16 : la grille doit tenir dedans
	if (Grid.Dims.X > MAX_int16 || Grid.Dims.Y > MAX_int16 || Grid.Dims.Z > MAX_int16)
	{
		UE_LOG(LogVoxelEditor, Error, TEXT("CreateVoxelBodyAsset : grille %s trop grande pour des coordonnees int16"), *Grid.Dims.ToString());
		return nullptr;
	}

	// ---------- 2. Nom et emplacement ----------
	// "/Game/Characters/SKM_StylizedFemale" -> dossier "/Game/Characters"
	const FString FolderPath = FPackageName::GetLongPackagePath(SourceMesh.GetPackage()->GetName());
	const FString AssetName = TEXT("VB_") + SourceMesh.GetName();      // VB = VoxelBody
	const FString PackageName = FolderPath / AssetName;                 // "/Game/Characters/VB_SKM_StylizedFemale"
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;    // chemin complet de l'objet DANS le package

	// ---------- 3. Remplissage dans des variables locales ----------
	// On ne touche à l'asset qu'à la fin : en cas d'erreur en cours de route, un asset existant reste intact.
	const FReferenceSkeleton& RefSkeleton = SourceMesh.GetRefSkeleton();

	TArray<FVoxelBodyCell> NewCells;
	TArray<FName>          NewBoneNames;
	TMap<int16, uint8>     BoneToSlot;   // indice d'os du squelette -> indice dans NewBoneNames

	for (int32 K = 0; K < Grid.Dims.Z; ++K)
	{
		for (int32 J = 0; J < Grid.Dims.Y; ++J)
		{
			for (int32 I = 0; I < Grid.Dims.X; ++I)
			{
				const int16 Bone = Grid.Bone[Grid.Index(I, J, K)];
				if (Bone == INDEX_NONE)
				{
					continue;   // cellule vide : la liste creuse ne la stocke pas
				}

				// Slot de cet os : déjà connu, ou nouvel os à ajouter à la table des noms
				uint8 Slot = 0;
				if (const uint8* ExistingSlot = BoneToSlot.Find(Bone))
				{
					Slot = *ExistingSlot;
				}
				else
				{
					if (NewBoneNames.Num() > MAX_uint8)   // 256 os déjà utilisés : plus de slot uint8 libre
					{
						UE_LOG(LogVoxelEditor, Error, TEXT("CreateVoxelBodyAsset : plus de %d os utilises, BoneSlot (uint8) insuffisant"), MAX_uint8 + 1);
						return nullptr;
					}
					Slot = static_cast<uint8>(NewBoneNames.Add(RefSkeleton.GetBoneName(Bone)));
					BoneToSlot.Add(Bone, Slot);
				}

				FVoxelBodyCell Cell;
				Cell.I = static_cast<int16>(I);
				Cell.J = static_cast<int16>(J);
				Cell.K = static_cast<int16>(K);
				Cell.BoneSlot = Slot;
				NewCells.Add(Cell);
			}
		}
	}

	// ---------- 4. Asset : récupérer l'existant ou en créer un ----------
	// LoadObject cherche d'abord en mémoire, puis sur le disque. NoWarn | Quiet : ne pas le trouver est un cas normal.
	UVoxelBodyAsset* Asset = LoadObject<UVoxelBodyAsset>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const bool bIsNewAsset = (Asset == nullptr);

	if (bIsNewAsset)
	{
		UPackage* Package = CreatePackage(*PackageName);
		// RF_Public : référençable hors de son package ; RF_Standalone : pas détruit par le GC sans référence
		Asset = NewObject<UVoxelBodyAsset>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	// ---------- 5. Validation : on écrit tout d'un coup ----------
	Asset->CellSize = Grid.CellSize;
	Asset->Origin = Grid.Origin;
	Asset->Dims = Grid.Dims;
	Asset->Skeleton = SourceMesh.GetSkeleton();
	Asset->Cells = MoveTemp(NewCells);       // MoveTemp : on transfère le tableau au lieu de le copier
	Asset->BoneNames = MoveTemp(NewBoneNames);

	// ---------- 6. Signaler l'asset à l'éditeur ----------
	if (bIsNewAsset)
	{
		FAssetRegistryModule::AssetCreated(Asset);   // le fait apparaître dans le Content Browser
	}
	Asset->MarkPackageDirty();                       // astérisque « non sauvegardé » : Ctrl+S manuel pour l'instant

	UE_LOG(LogVoxelEditor, Log, TEXT("CreateVoxelBodyAsset : %s %s | %d cellules | %d os"),
		*ObjectPath, bIsNewAsset ? TEXT("cree") : TEXT("mis a jour"),
		Asset->Cells.Num(), Asset->BoneNames.Num());

	return Asset;
}
