# Edge Diagnostics Adapter

Edge Diagnostics Adapter is a protocol adaptor that enables tools to debug and diagnose Edge using the [Chrome Debugging Protocol](https://chromedevtools.github.io/debugger-protocol-viewer/).

## Installation

For now we provide binaries on our [releases page](https://github.com/Microsoft/EdgeDiagnosticsAdapter/releases) and via NPM.

#### Install via NPM
```npm install edge-diagnostics-adapter```

Binary is now located in` ./node_modules/edge-diagnostics-adapter/dist/<platform>`

## Usage

```console
./<path>/EdgeDiagnosticsAdaptor.exe
```

* `--launch <url>` for opening Edge with a specific URLt.
* `Ctrl-C` to quit. Also, the adaptor can be left running as a background process.


#### Usage via node.
To simplify usage with Node we are also providing [edge-diagnostics-launch](https://github.com/Microsoft/edge-diagnostics-launch), that works a simple wrapper for the Edge Diagnostics Adapter.

#### View and inspect debuggable targets

Navigate to [localhost:9222](http://localhost:9222). You'll see a listing of all connected devices.


## Supported features and API

| Area  | Method    |
|-------|-----------|
|  DOM  | getDocument
|  DOM  | getAttributes
|  DOM  | hideHighlight
|  DOM  | highlightNode
|  DOM  | setInspectModeEnabled
|  DOM  | requestChildNodes
|  DOM  | pushNodesByBackendIdsToFrontend
|  DOM  | pushNodeByPathToFrontend
|  DOM  | pushNodeByPathToFrontend
|  CSS  | getInlineStylesForNode
|  CSS  | getMatchedStylesForNode
|  CSS  | getComputedStyleForNode
|  CSS  | getStyleSheetText
|  CSS  | setPropertyText
|  Page | enable
|  Page | reload
|  Page | navigate
|  Page | getCookies
|  Page | getResourceTree
|  Page | getAnimationsPlaybackRate
|  Page | getNavigationHistory
|  Page | deleteCookie
|  Page | setOverlayMessage
|  Page | canScreencast
|  Page | canEmulate
|  Page | screencastFrameAck
|  Page | startRecordingFrames
|  Page | stopRecordingFrames
|  Page | startScreencast
|  Page | stopScreencast
|  Page | setShowViewportSizeOnResize
| Runtime | enable
| Runtime | evaluate
| Runtime | callFunctionOn
| Runtime | getProperties
| Script | canSetScriptSource
| Script | disable
| Script | enable
| Script | evaluateOnCallFrame
| Script | getScriptSource
| Script | removeBreakpoint
| Script | pause
| Script | resume
| Script | setBreakpointByUrl
| Script | stepInto
| Script | stepOut
| Script | stepOver


## Building & Contributing
To build and contribute to this project take a gander at the wiki pages on [building](https://github.com/Microsoft/EdgeDiagnosticsAdapter/wiki/Building) and contributing (coming soon).
