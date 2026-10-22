     using UnrealBuildTool;
	 
	     public class VoxelModuleEditor: ModuleRules
	     {
	          public VoxelModuleEditor(ReadOnlyTargetRules Target) : base(Target)
	          {
	                PrivateDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "ToolMenus", "VoxelModule", "Slate", "SlateCore", "ContentBrowser", "MeshDescription", "SkeletalMeshDescription", "AssetRegistry"});
	          }
	     }