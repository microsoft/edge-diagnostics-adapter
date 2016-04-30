//
// Copyright (C) Microsoft. All rights reserved.
//

import {EdgeAdapter} from './EdgeAdapterService';

var service = new EdgeAdapter.Service();
service.run(8080, 9223);
console.log("Edge Diagnostics Adapter listening on port 8080...");