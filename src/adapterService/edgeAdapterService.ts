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

import {IChromeInstance} from '../../lib/EdgeAdapterInterfaces.d.ts'
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

            if (url === ('/json') || url === "/json/list") {
                // Respond with json
                response.writeHead(200, { "Content-Type": "text/json" });
                response.write(JSON.stringify(this.getEdgeJson()));
                response.end();
            } else if (url === "/json/version") {
                // Get the version
                response.writeHead(200, { "Content-Type": "text/json" });
                response.write(JSON.stringify(this.getEdgeVersionJson()));
                response.end();
            } else if (url === "/protocol.json") {
                // Write out protocol.json file
                response.writeHead(200, { "Content-Type": "text/json" });
                response.write(this.getChromeProtocol());
                response.end();
            } else if (url === "") {
                // Respond with attach page
                response.writeHead(200, { "Content-Type": "text/html" });
                response.write(fs.readFileSync(__dirname + '/../chromeProtocol/inspect.html', 'utf8'));
                response.end();
            } else {
                // Not found
                response.writeHead(404, { "Content-Type": "text/html" });
                response.end();
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
                { engine: "browser", filename: "Assert.js" },
                { engine: "browser", filename: "Common.js" },
                { engine: "browser", filename: "Browser.js" },
                { engine: "browser", filename: "DOM.js" },
                { engine: "browser", filename: "Runtime.js" },
                { engine: "browser", filename: "Page.js" },
                { engine: "browser", filename: "CSSParser.js" },
                { engine: "browser", filename: "BrowserTool.js" },
                { engine: "debugger", filename: "Assert.js" },
                { engine: "debugger", filename: "Common.js" },
                { engine: "debugger", filename: "Debugger.js" },
            ];

            for (let i = 0; i < files.length; i++) {
                const script = fs.readFileSync(__dirname + '/../chromeProtocol/' + files[i].filename, 'utf8');
                this.log(`Injecting '${files[i].engine}:${files[i].filename}'`);
                edgeAdapter.injectScriptTo(instanceId, files[i].engine, files[i].filename, script);
            }
        }

        private getEdgeJson(): IChromeInstance[] {
            const chromeInstances: IChromeInstance[] = [];
            const host = "localhost";
            const map = new Map<string, string>();

            const instances = edgeAdapter.getEdgeInstances();
            for (let i = 0; i < instances.length; i++) {
                // Get or generate a new guid
                let guid: string = null;
                if (!this._guidToIdMap.has(instances[i].id)) {
                    guid = this.createGuid();
                } else {
                    guid = this._guidToIdMap.get(instances[i].id);
                }

                map.set(guid, instances[i].id);

                const websocket = `ws://${host}:${this._serverPort}/devtools/page/${guid}`;
                const devtools = `http://${host}:${this._chromeToolsPort}/devtools/inspector.html?ws=${websocket.substr(5)}`;

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

        private getEdgeVersionJson(): any {
            // Todo: Currently Edge does not store it's UA string and there is no  way to fetch the UA without loading Edge.
            const userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2486.0 Safari/537.36 Edge/13.10586";
            return {
                Browser: "Microsoft Edge 13",
                "Protocol-Version": "1",
                "User-Agent": userAgent,
                "WebKit-Version": "0"
            }
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
