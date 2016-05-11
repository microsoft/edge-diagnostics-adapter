//
// Copyright (C) Microsoft. All rights reserved.
//

import {argv} from 'yargs';
import {EdgeAdapter} from './adapterService/EdgeAdapterService';

if (argv.help || argv["?"]) {
    console.log("node edgeAdapter.js <options>");
    console.log("<options>:");
    console.log(" --port=9999             The port to use for the edgeAdapter endpoint. (default 9222)");
    console.log(" --servetools<=9999>     Serve up an instance of the chrome devtools (must be installed)");
    console.log("                         at the optional specified port (default 9223).")
    console.log(" --url<=http://foo.com>  Launch a new instance of Edge to the optionally specified url (default http://www.bing.com)");
    console.log("");
    console.log("Example:");
    console.log("node edgeAdapter.js");
    console.log("node edgeAdapter.js --port=8080 --servetools");
    console.log("node edgeAdapter.js --url=http://www.msn.com");
} else {
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
}
