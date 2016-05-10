//
// Copyright (C) Microsoft. All rights reserved.
//

import {argv} from 'yargs';
import {EdgeAdapter} from './EdgeAdapterService';

const port = (argv.port ? argv.port : 9222);
const url = (argv.url ? argv.url : null);

let chromeToolsPort = 0;
if (argv.servetools) {
    if (typeof argv.servetools === 'number') {
        chromeToolsPort = argv.servetools;
    } else {
        chromeToolsPort = 9223
    }
}

var service = new EdgeAdapter.Service();
service.run(port, chromeToolsPort, url);
console.log(`Edge Diagnostics Adapter listening on port ${port}...`);
