// MG Dyanmic Navigation plugin Created by Cem Akkaya licensed under MIT.

#pragma once

#include "Modules/ModuleManager.h"

class FMGDynamicNavigationModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
