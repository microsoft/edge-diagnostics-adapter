//
// Copyright (C) Microsoft. All rights reserved.
//

/// <reference path="Edge.DiagnosticOM.d.ts" />
/// <reference path="Interfaces.d.ts"/>

/// Proxy to hande the page domain of the Chrome remote debug protocol 
module EdgeDiagnosticsAdapter {
    export class RuntimeHandler implements IDomainHandler {
        constructor() {
        }

        public processMessage(method: string, request: IWebKitRequest): void {
            var processedResult: IWebKitResult;
            let isAsync: boolean = false;

            switch (method) {
                case "enable":
                    processedResult = { result: {} };
                    break;

                case "evaluate":
                case "callFunctionOn":
                    var resultFromEval: any;
                    var wasThrown = false;

                    if (method === "evaluate" && request.params.expression) {
                        try {
                            var escapedInput = JSON.stringify(request.params.expression).slice(1, -1);
                            resultFromEval = browser.executeScript(escapedInput);

                            if (request.params.awaitPromise == true) {
                                isAsync = true;
                                resultFromEval.then(v => {
                                    processedResult = this.createProcessedResult(v, false);
                                    browserHandler.postResponse(request.id, processedResult);
                                }).catch( e => {
                                    processedResult = this.createProcessedResult(e, true);
                                    browserHandler.postResponse(request.id, processedResult); 
                                });
                            }
                        } catch (e) {
                            resultFromEval = e;
                            wasThrown = true;
                        }
                    } else if (method === "callFunctionOn" && request.params.functionDeclaration) {
                        var args = [];
                        if (request.params.arguments) {
                            args.push(" ");
                            for (var i = 0; i < request.params.arguments.length; i++) {
                                var arg = request.params.arguments[i].value;
                                args.push(JSON.stringify(arg));
                            }
                        }

                        try {
                            var command = request.params.functionDeclaration + ".call(window" + args.join(",") + ")";
                            var escapedInput = JSON.stringify(command).slice(1, -1);
                            resultFromEval = browser.executeScript(escapedInput);
                        } catch (e) {
                            resultFromEval = e;
                            wasThrown = true;
                        }
                    }

                    if (!isAsync) {
                        processedResult = this.createProcessedResult(resultFromEval, wasThrown); 
                    }                   
                    break;

                default:
                    processedResult = null;
                    break;
            }

            if (!isAsync) {
                browserHandler.postResponse(request.id, processedResult);
            }
        }

        private createProcessedResult(resultFromEval: any, exeptionWasThrown: boolean) : IWebKitResult{
            var id = null;
            var description = (resultFromEval ? resultFromEval.toString() : "");
            var value = resultFromEval;
            var processedResult: IWebKitResult;

            if (resultFromEval && typeof resultFromEval === "object") {
                        id = "1";
                        description = "Object";
                        value = null;
                    }

                    var resultDesc = {
                        objectId: id,
                        type: "" + typeof value,
                        value: value,
                        description: description
                    };

                    processedResult = {
                        result: {
                            wasThrown: exeptionWasThrown,
                            result: resultDesc
                        }
                    };
            return processedResult;
        }
    }   

    export var runtimeHandler: RuntimeHandler = new RuntimeHandler();
}