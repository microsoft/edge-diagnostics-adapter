//
// Copyright (C) Microsoft. All rights reserved.
//

/// <reference path="Edge.DiagnosticOM.d.ts" />
/// <reference path="Interfaces.d.ts"/>
/// <reference path="Browser.ts"/>

/// Proxy to handle the network domain of the Chrome remote debug protocol
module EdgeDiagnosticsAdapter
{
    export class NetworkHandler implements IDomainHandler {

        public processMessage(method: string, request: IWebKitRequest): void {
            var processedResult: IWebKitResult;

            switch (method) {
                case "clearBrowserCache":
                    processedResult = this.clearCache();
                    break;

                case "setCacheDisabled":
                    processedResult = this.disableCache(request);
                    break;

                case "setUserAgentOverride":
                    processedResult = this.setActiveUserAgent(request);
                    break;

                default:
                    break;
            }

            browserHandler.postResponse(request.id, processedResult);
        }

        private clearCache(): IWebKitResult {
            var processedResult: IWebKitResult = {};

            try {
                resources.clearBrowserCache();                
            } catch (ex) {
                processedResult = {
                    error: ex
                };
            }

            return processedResult;
        }

        private disableCache(request: IWebKitRequest): IWebKitResult {
            var processedResult: IWebKitResult = {};

            try {
                resources.alwaysRefreshFromServer =  request.params.cacheDisabled;         
            } catch (ex) {
                processedResult = {
                    error: ex
                };
            }

            return processedResult;
        }

        private setActiveUserAgent(request: IWebKitRequest): IWebKitResult {
            var processedResult: IWebKitResult = {};
           
            try {                
                emulation.userAgentStringManager.current = request.params.userAgent;                 
            } catch (ex) {
                processedResult = {
                    error: ex
                };
            }

            return processedResult;
        }
    } 

    export var networkHandler: NetworkHandler = new NetworkHandler();   
}