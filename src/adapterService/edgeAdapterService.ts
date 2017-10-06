///<reference path='../../lib/Addon.node.d.ts' />
//
// Copyright (C) Microsoft. All rights reserved.
//

import { IChromeInstance } from '../../lib/EdgeAdapterInterfaces';
import * as edgeAdapter from '../../lib/Addon.node';
// const binding_path = binary.find(path.resolve(path.join(__dirname,'../../../package.json')));
// var edgeAdapter = require(binding_path);

import * as http from 'http';
import * as ws from 'ws';
import * as fs from 'fs';
import * as crypto from 'crypto';
import { Server as WebSocketServer } from 'ws';

declare var __dirname;

export module EdgeAdapter {
    export class Service {
        private _httpServer: http.Server;
        private _webSocketServer: WebSocketServer;
        private _serverPort: number;
        private _chromeToolsPort: number;
        private _guidToIdMap: Map<string, string> = new Map<string, string>();
        private _idToEdgeMap: Map<string, edgeAdapter.EdgeInstanceId> = new Map<string, edgeAdapter.EdgeInstanceId>();
        private _idToNetWorkProxyMap: Map<string, edgeAdapter.NetworkProxyInstanceId> = new Map<string, edgeAdapter.NetworkProxyInstanceId>();
        private _edgeToWSMap: Map<edgeAdapter.EdgeInstanceId, ws[]> = new Map<edgeAdapter.EdgeInstanceId, ws[]>();
        private _diagLogging: boolean = false;
        private _newtworkProxyImplementedRequests: Array<string> = new Array<string>('Network.enable', 'Network.disable', 'Network.getResponseBody');

        constructor(diagLogging: boolean) {
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
            this._webSocketServer.on('connection', (client, message) => this.onWSSConnection(client, message));

            this._httpServer.listen(serverPort, "0.0.0.0");
        }

        private onServerRequest(request: http.IncomingMessage, response: http.ServerResponse): void {
            // Normalize request url
            let url = request.url.trim().toLocaleLowerCase();
            // Extract parameter list
            // TODO: improve the parameter extraction
            let urlParts = this.extractParametersFromUrl(url);
            url = urlParts.url;
            let param = urlParts.paramChain;
            if (url.lastIndexOf('/') == url.length - 1) {
                url = url.substr(0, url.length - 1);
            }

            const host = request.headers.host || "localhost";

            switch (url) {
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

                case '/json/new':
                    // create a new tab
                    if (!param) {
                        param = "";
                    }
                    this.createNewTab(param, host, response)
                    break;

                case '/json/close':
                    // close a tab
                    response.writeHead(200, { "Content-Type": "text/html" });
                    this.closeEdgeInstance(param);
                    response.end();
                    break;

                default:
                    // Not found
                    response.writeHead(404, { "Content-Type": "text/html" });
                    response.end();
                    break;
            }
        }

        private createNewTab(param: string, host: any, response: http.ServerResponse) {
            let initialChromeTabs = this.getEdgeJson(host);
            let retries = 200; // Retry for 30 seconds.

            if (edgeAdapter.openEdge(param)) {
                const getNewTab = () => {
                    let actualChromeTabs = this.getEdgeJson(host);
                    let newTabInfo = this.getNewTabInfo(initialChromeTabs, actualChromeTabs, param);

                    if (!newTabInfo && retries > 0) {
                        retries--;

                        return setTimeout(getNewTab, 150);
                    }

                    response.write(JSON.stringify(newTabInfo));
                    response.end();
                }

                return getNewTab();
            } else {
                response.end();
            }
        }

        private getNewTabInfo(initialInstances: IChromeInstance[], actualInstances: IChromeInstance[], url: string): IChromeInstance {
            for (var index = 0; index < actualInstances.length; index++) {
                let element = actualInstances[index];
                if (!initialInstances.some(x => x.id == element.id)) {
                    return element;
                }
            }

            return null;
        }

        private onWSSConnection(ws: ws, message?: http.IncomingMessage): void {
            // Normalize request url
            if (!message) {
                return;
            }
            let url = message.url.trim().toUpperCase();
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
            let networkInstanceId: edgeAdapter.NetworkProxyInstanceId = null;

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
                networkInstanceId = this._idToNetWorkProxyMap.get(id);
                if (!networkInstanceId) {
                    networkInstanceId = edgeAdapter.createNetworkProxyFor(id);
                    if (networkInstanceId) {
                        this._idToNetWorkProxyMap.set(id, networkInstanceId);
                        this._idToNetWorkProxyMap.set(networkInstanceId, id);
                    }
                }
            }

            if (succeeded && networkInstanceId) {
                // Forward messages to the proxy
                ws.on('message', (msg: ws.Data) => {
                    if (this._diagLogging) {
                        console.log("Client:", instanceId, msg);
                    }
                    if (this.isMessageForNetworkProxy(msg)) {
                        edgeAdapter.forwardTo(networkInstanceId, msg.toString());
                    } else {
                        edgeAdapter.forwardTo(instanceId, msg.toString());
                    }
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

        private isMessageForNetworkProxy(requestMessage: ws.Data): boolean {
            var parsedMessage: Object;
            try {
                parsedMessage = JSON.parse(requestMessage.toString());
            } catch (SyntaxError) {
                console.log("Error parsing request message: ", requestMessage);
                return false;
            }
            let requestType = parsedMessage['method'] as string;

            return this._newtworkProxyImplementedRequests.indexOf(requestType) != -1;
        }

        private onEdgeMessage(instanceId: edgeAdapter.EdgeInstanceId, msg: string): void {
            if (this._diagLogging) {
                console.log("EdgeService:", instanceId, msg);
            }

            let edgeProxyId;
            if (this._idToNetWorkProxyMap.has(instanceId)) {
                // message comes from network proxy, we get he edge proxy id
                const id = this._idToNetWorkProxyMap.get(instanceId)
                edgeProxyId = this._idToEdgeMap.get(id);
            }
            else {
                edgeProxyId = instanceId;
            }

            if (this._edgeToWSMap.has(edgeProxyId)) {
                const sockets = this._edgeToWSMap.get(edgeProxyId)
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

        private getEdgeJson(host: string | string[]): IChromeInstance[] {
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

        private getChromeProtocol(): string {
            const script = fs.readFileSync(__dirname + '/../chromeProtocol/protocol.json', 'utf8');
            return script;
        }

        private createGuid(): string {
            const g: string = crypto.createHash('md5').update(Math.random().toString()).digest('hex').toUpperCase();
            return `${g.substring(0, 8)}-${g.substring(9, 13)}-${g.substring(13, 17)}-${g.substring(17, 21)}-${g.substring(21, 31)}`
        }

        private extractParametersFromUrl(url: string): { url: string, paramChain: string } {
            let parameters;
            const closeUrl = '/json/close';

            if (url.indexOf('/json/new') != -1) {
                const urlSegements = url.split('?');
                if (urlSegements.length > 1) {
                    url = urlSegements[0];
                    parameters = urlSegements[1];
                }
            } else if (url.indexOf(closeUrl) != -1) {
                parameters = url.replace(`${closeUrl}/`, '');
                url = url.slice(0, closeUrl.length);
            }

            return { url: url, paramChain: parameters };
        }

        private closeEdgeInstance(guid: string): boolean {
            var edgeResult = false;
            var networkProxyResult = false;

            const id = this._guidToIdMap.get(guid.toLocaleUpperCase());
            if (id) {
                edgeResult = edgeAdapter.closeEdge(id);
                const networkInstanceId = this._idToNetWorkProxyMap.get(id);
                if (networkInstanceId) {
                    networkProxyResult = edgeAdapter.closeNetworkProxyInstance(networkInstanceId);
                }
                // tab is closed, clean all the mappings and close connections
                let instanceId = this._idToEdgeMap.get(id);
                const sockets = this._edgeToWSMap.get(instanceId)
                if (sockets) {
                    for (let i = 0; i < sockets.length; i++) {
                        sockets[i].removeAllListeners();
                        sockets[i].close();
                    }
                }
                this._edgeToWSMap.delete(instanceId);
                this._guidToIdMap.delete(guid.toLocaleUpperCase());
                this._guidToIdMap.delete(id);
                this._idToNetWorkProxyMap.delete(id);
                this._idToNetWorkProxyMap.delete(networkInstanceId);
                this._idToEdgeMap.delete(id);
            }
            return edgeResult && networkProxyResult;
        }
    }
}
