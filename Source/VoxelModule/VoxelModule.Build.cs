using System.IO;
using UnrealBuildTool;

public class VoxelModule : ModuleRules
{
    public VoxelModule(ReadOnlyTargetRules Target) : base(Target)
    {
        // Modules dont tu as besoin
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",                  // ← OBLIGATOIRE pour Misc/PlatformMisc.h
                "CoreUObject",
                "Engine"
            }
        );

        string FastNoiseLite = Path.Combine(ModuleDirectory, "../ThirdParty/FastNoiseLite");
        PublicIncludePaths.Add(FastNoiseLite);

        // Si ton module doit exposer des API à l’extérieur :
        PublicDependencyModuleNames.AddRange(
            new string[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent", "GeometryFramework" }
        );

        // Configuration supplémentaire si nécessaire
    }
}