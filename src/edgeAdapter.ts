//
// Copyright (C) Microsoft. All rights reserved.
//

import {argv} from 'yargs';
import {EdgeAdapter} from './adapterService/edgeAdapterService';

if (argv.h || argv.help || argv["?"]) {
    console.log("node edgeAdapter.js <options>");
    console.log("<options>:");
    console.log(" -p, --port=9999             The port to use for the edgeAdapter endpoint. (default 9222)");
    console.log(" --servetools<=9999>     Serve up an instance of the chrome devtools (must be installed)");
    console.log("                         at the optional specified port (default 9223).")
    console.log(" -u, --url<=http://foo.com>  Launch a new instance of Edge to the optionally specified url (default http://www.bing.com)");
    console.log(" -d, --diagnostics       Verbose log requests from client websocket and responses from service");
    console.log(" -h, --help              Display this help options");
    console.log("");
    console.log("Example:");
    console.log("node edgeAdapter.js");
    console.log("node edgeAdapter.js --port=8080 --servetools");
    console.log("node edgeAdapter.js --url=http://www.msn.com");
} else {
    const port = argv.port || argv.p || 9222;
    const url = argv.url || argv.u || null;
    const diagLogging = argv.diagnostics || argv.d || false;

    let chromeToolsPort = 0;
    if (argv.servetools) {
        if (typeof argv.servetools === 'number') {
            chromeToolsPort = argv.servetools;
        } else {
            chromeToolsPort = 9223
        }
    }

    var service = new EdgeAdapter.Service(diagLogging);
    service.run(port, chromeToolsPort, url);
    console.log(`Edge Diagnostics Adapter listening on port ${port}...`);
}
