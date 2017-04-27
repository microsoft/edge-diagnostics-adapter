//
// Copyright (C) Microsoft. All rights reserved.
//

#pragma once

#include "resource.h"
#include <atlbase.h>
#include "NetworkListener_h.h"

class CNetworkListenerModule : public CAtlDllModuleT<CNetworkListenerModule>
{
public:
	DECLARE_LIBID(LIBID_NetworkListenerLib)
};

extern class CNetworkListenerModule _AtlModule;
