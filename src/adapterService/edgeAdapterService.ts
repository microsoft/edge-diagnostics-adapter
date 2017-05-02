//
// Copyright (C) Microsoft. All rights reserved.
//

// Override require to pick the correct addon architecture at runtime
(function() {
    var originalReq = require;
    require = <any>function(path: string) {
        if (path !== "../../lib/Addon.node") {
            return originalReq(path);
        } else {
            let mod;
            try {
                mod = originalReq(path);
            } catch (ex) {
                try {
                    mod = originalReq("../../lib/Addon64.node");
                } catch (ex) {
                    // Potential node version mismatch error
                    console.log(ex.message, ex.stack);
                }
            }
            return mod;
        }
    }
})();

import {IChromeInstance} from '../../lib/EdgeAdapterInterfaces'
import * as edgeAdapter from '../../lib/Addon.node';
import * as http from 'http';
import * as ws from 'ws';
import * as fs from 'fs';
import * as crypto from 'crypto';
import {Server as WebSocketServer} from 'ws';

declare var __dirname;

export module EdgeAdapter {
    export class Service {
        private _httpServer: http.Server;
        private _webSocketServer: WebSocketServer;
        private _serverPort: number;
        private _chromeToolsPort: number;
        private _guidToIdMap: Map<string, string> = new Map<string, string>();
        private _idToEdgeMap: Map<string, edgeAdapter.EdgeInstanceId> = new Map<string, edgeAdapter.EdgeInstanceId>();
        private _edgeToWSMap: Map<edgeAdapter.EdgeInstanceId, ws[]> = new Map<edgeAdapter.EdgeInstanceId, ws[]>();
        private _diagLogging: boolean = false;

        constructor (diagLogging: boolean) {
            this._diagLogging = diagLogging;
        }

        public run(serverPort: number, chromeToolsPort: number, url: string): void {
            this._serverPort = serverPort;
            this._chromeToolsPort = chromeToolsPort;

            edgeAdapter.initialize(__dirname, (id, msg) => this.onEdgeMessage(id, msg), (msg) => this.onLogMessage(msg));
            edgeAdapter.setSecurityACLs(__dirname + "\\..\\..\\lib\\");

            if (chromeToolsPort > 0) {
                edgeAdapter.serveChromeDevTools(chromeToolsPort);
            }

            if (url) {
                edgeAdapter.openEdge(url);
            }

            this._httpServer = http.createServer((req, res) => this.onServerRequest(req, res));
            this._webSocketServer = new WebSocketServer({ server: this._httpServer });
            this._webSocketServer.on('connection', (client) => this.onWSSConnection(client));

            this._httpServer.listen(serverPort, "0.0.0.0");
        }

        private onServerRequest(request: http.IncomingMessage, response: http.ServerResponse): void {
            // Normalize request url
            let url = request.url.trim().toLocaleLowerCase();
            if (url.lastIndexOf('/') == url.length - 1) {
                url = url.substr(0, url.length - 1);
            }

            const host = request.headers.host || "localhost";

            switch(url){
                case '/json':
                case '/json/list':
                    // Respond with json
                    response.writeHead(200, { "Content-Type": "text/json" });
                    response.write(JSON.stringify(this.getEdgeJson(host)));
                    response.end();
                    break;

                case '/json/version':
                    // Write out protocol.json file
                    response.writeHead(200, { "Content-Type": "text/json" });
                    response.write(this.getEdgeVersionJson());
                    response.end();
                    break;

                case '/json/protocol':
                    // Write out protocol.json file
                    response.writeHead(200, { "Content-Type": "text/json" });
                    response.write(this.getChromeProtocol());
                    response.end();
                    break;

                case '':
                    // Respond with attach page
                    response.writeHead(200, { "Content-Type": "text/html" });
                    response.write(fs.readFileSync(__dirname + '/../chromeProtocol/inspect.html', 'utf8'));
                    response.end();
                    break;

                default:
                    // Not found
                    response.writeHead(404, { "Content-Type": "text/html" });
                    response.end();
                    break;
            }
        }

        private onWSSConnection(ws: ws): void {
            // Normalize request url
            let url = ws.upgradeReq.url.trim().toUpperCase();
            if (url.lastIndexOf('/') == url.length - 1) {
                url = url.substr(0, url.length - 1);
            }

            let guid = url;
            const index = guid.lastIndexOf('/');
            if (index != -1) {
                guid = guid.substr(index + 1);
            }

            let succeeded = false;
            let instanceId: edgeAdapter.EdgeInstanceId = null;

            if (this._guidToIdMap.has(guid)) {
                const id = this._guidToIdMap.get(guid);
                instanceId = this._idToEdgeMap.get(id);
                if (!instanceId) {
                    // New connection
                    instanceId = edgeAdapter.connectTo(id);
                    if (instanceId) {
                        this.injectAdapterFiles(instanceId);
                        this._idToEdgeMap.set(id, instanceId);
                        this._edgeToWSMap.set(instanceId, [ws]);
                        succeeded = true;
                    }
                } else {
                    // Already connected
                    const sockets = this._edgeToWSMap.get(instanceId);
                    sockets.push(ws);
                    this._edgeToWSMap.set(instanceId, sockets);
                    succeeded = true;
                }
            }

            if (succeeded) {
                // Forward messages to the proxy
                ws.on('message', (msg) => {
                    if (this._diagLogging) {
                        console.log("Client:", instanceId, msg);
                    }
                    edgeAdapter.forwardTo(instanceId, msg);
                });

                const removeSocket = (instanceId: edgeAdapter.EdgeInstanceId) => {
                    const sockets = this._edgeToWSMap.get(instanceId);
                    const index = sockets.indexOf(ws);
                    if (index > -1) {
                        sockets.splice(index, 1);
                    }
                    this._edgeToWSMap.set(instanceId, sockets);
                };

                // Remove socket on close or error
                ws.on('close', () => {
                    removeSocket(instanceId);
                });
                ws.on('error', () => {
                    removeSocket(instanceId);
                });
            } else {
                // No matching Edge instance
                ws.close();
            }
        }

        private log(message: string): void {
            this.onLogMessage(message);
        }

        private onEdgeMessage(instanceId: edgeAdapter.EdgeInstanceId, msg: string): void {
            if (this._diagLogging) {
                console.log("EdgeService:", instanceId, msg);
            }
            if (this._edgeToWSMap.has(instanceId)) {
                const sockets = this._edgeToWSMap.get(instanceId)
                for (let i = 0; i < sockets.length; i++) {
                    sockets[i].send(msg);
                }
            }
        }

        private onLogMessage(msg: string): void {
            console.log("Log: " + msg);
        }

        private injectAdapterFiles(instanceId: edgeAdapter.EdgeInstanceId): void {
            const files: { engine: edgeAdapter.EngineType, filename: string }[] = [
                { engine: "browser", filename: "assert.js" },
                { engine: "browser", filename: "common.js" },
                { engine: "browser", filename: "browser.js" },
                { engine: "browser", filename: "dom.js" },
                { engine: "browser", filename: "runtime.js" },
                { engine: "browser", filename: "page.js" },
                { engine: "browser", filename: "CssParser.js" },
                { engine: "browser", filename: "browserTool.js" },
                { engine: "browser", filename: "network.js" },
                { engine: "debugger", filename: "assert.js" },
                { engine: "debugger", filename: "common.js" },
                { engine: "debugger", filename: "debugger.js" },                
            ];

            for (let i = 0; i < files.length; i++) {
                const script = fs.readFileSync(__dirname + '/../chromeProtocol/' + files[i].filename, 'utf8');
                this.log(`Injecting '${files[i].engine}:${files[i].filename}'`);
                edgeAdapter.injectScriptTo(instanceId, files[i].engine, files[i].filename, script);
            }
        }

        private getEdgeJson(host: string): IChromeInstance[] {
            const chromeInstances: IChromeInstance[] = [];
            const map = new Map<string, string>();

            const instances = edgeAdapter.getEdgeInstances();
            if (this._diagLogging) {
                console.log("Edge Instances:", instances);
            }

            for (let i = 0; i < instances.length; i++) {
                // Get or generate a new guid
                let guid: string = null;
                if (!this._guidToIdMap.has(instances[i].id)) {
                    guid = this.createGuid();
                } else {
                    guid = this._guidToIdMap.get(instances[i].id);
                }

                map.set(guid, instances[i].id);
                map.set(instances[i].id, guid);

                const websocket = `ws://${host}/devtools/page/${guid}`;
                const devtools = `chrome-devtools://devtools/bundled/inspector.html?ws=${websocket.substr(5)}`;

                // Generate the json description of this instance
                chromeInstances.push({
                    description: instances[i].processName,
                    devtoolsFrontendUrl: devtools,
                    id: guid,
                    title: instances[i].title,
                    type: "page",
                    url: instances[i].url,
                    webSocketDebuggerUrl: websocket
                });
            }

            // Reset the map to the new instances
            this._guidToIdMap = map;

            return chromeInstances;
        }

        private getEdgeVersionJson(): string {
            // Todo: Currently Edge does not store it's UA string and there is no  way to fetch the UA without loading Edge.
            const userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2486.0 Safari/537.36 Edge/13.10586";
            const version = {
                "Browser": "Microsoft Edge 13",
                "Protocol-Version": "1",
                "User-Agent": userAgent,
                "WebKit-Version": "0"
            };
            return JSON.stringify(version);
        }

        private getChromeProtocol():string {
            const script = fs.readFileSync(__dirname + '/../chromeProtocol/protocol.json', 'utf8');
            return script;
        }

        private createGuid(): string {
            const g: string = crypto.createHash('md5').update(Math.random().toString()).digest('hex').toUpperCase();
            return `${g.substring(0, 8)}-${g.substring(9, 13)}-${g.substring(13, 17)}-${g.substring(17, 21)}-${g.substring(21, 31)}`
        }
    }
}
