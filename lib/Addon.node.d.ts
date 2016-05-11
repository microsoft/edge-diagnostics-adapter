export type EdgeInstanceId = string;
export type EngineType = "browser" | "debugger";
export function initialize(rootPath: string, onEdgeMessage: (instanceId: EdgeInstanceId, msg: string) => void, onLogMessage: (msg: string) => void): boolean;
export function getEdgeInstances(): { id: string, url: string, title: string, processName: string }[];
export function setSecurityACLs(filepath: string): boolean;
export function openEdge(url: string): boolean;
export function killAll(exeName: string): boolean;
export function serveChromeDevTools(port: number): boolean;
export function connectTo(id: string): EdgeInstanceId;
export function injectScriptTo(instanceId: EdgeInstanceId, engine: EngineType, filename: string, script: string): void
export function forwardTo(instanceId: EdgeInstanceId, message: string): void;
