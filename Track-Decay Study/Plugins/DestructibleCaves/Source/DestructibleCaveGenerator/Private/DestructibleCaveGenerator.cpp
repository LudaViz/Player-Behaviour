// Copyright 2025 J2K2. All Rights Reserved.

#include "DestructibleCaveGenerator.h"

#define LOCTEXT_NAMESPACE "FDestructibleCaveGeneratorModule"

void FDestructibleCaveGeneratorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FDestructibleCaveGeneratorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDestructibleCaveGeneratorModule, DestructibleCaveGenerator)