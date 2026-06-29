// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FDestructibleCaveGeneratorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
