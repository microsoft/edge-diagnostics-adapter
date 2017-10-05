# Edge Diagnostics Adapter

[![Build status](https://ci.appveyor.com/api/projects/status/wo4fnnx8735xa07d/branch/master?svg=true)](https://ci.appveyor.com/project/molant/edge-diagnostics-adapter/branch/master)

Edge Diagnostics Adapter is a protocol adapter that enables tools to
debug and diagnose Edge using the [Chrome DevTools Protocol][cdp-protocol].

The latest version of Edge Diagnostics Adapter works in Windows 10 - 14393 x64.

## Installation and usage

You can install Edge Diagnostics Adapter via [`npm`][npm]:

```bash

npm install edge-diagnostics-adapter

```

Or downloading it from the [releases page][releases] in GitHub.

Once you have it installed locally you can execute it as follows:

```bash

node /path/to/edge-diagnostics-adapter/out/src/edgeAdapter.js --port=8080 --servetools

```

## View and inspect debuggable targets

Navigate to [localhost:9222][localhost]. You'll see a listing of all
debuggable targets.

## Supported features and API

The following API of the Chrome Debugger Protocol is supported:

| Area  | Method    |
|-------|-----------|
|  CSS  | getComputedStyleForNode
|  CSS  | getInlineStylesForNode
|  CSS  | getMatchedStylesForNode
|  CSS  | setPropertyText
|  CSS  | getStyleSheetText
| Debugger | canSetScriptSource
| Debugger | disable
| Debugger | enable
| Debugger | evaluateOnCallFrame
| Debugger | getScriptSource
| Debugger | pause
| Debugger | removeBreakpoint
| Debugger | resume
| Debugger | setBreakpointByUrl
| Debugger | stepInto
| Debugger | stepOut
| Debugger | stepOver
|  DOM  | getAttributes
|  DOM  | getDocument
|  DOM  | getOuterHTML
|  DOM  | hideHighlight
|  DOM  | highlightNode
|  DOM  | pushNodeByPathToFrontend
|  DOM  | pushNodesByBackendIdsToFrontend
|  DOM  | querySelector
|  DOM  | querySelectorAll
|  DOM  | requestChildNodes
|  DOM  | setInspectModeEnabled
| Network | enable
| Network | clearBrowserCache
| Network | setCacheDisabled
| Network | requestWillBeSent
| Network | responseReceived
| Network | getResponseBody
|  Page | canEmulate
|  Page | canScreencast
|  Page | deleteCookie
|  Page | enable
|  Page | getAnimationsPlaybackRate
|  Page | getCookies
|  Page | getNavigationHistory
|  Page | getResourceTree
|  Page | loadEventFired
|  Page | navigate
|  Page | reload
|  Page | setOverlayMessage
|  Page | setShowViewportSizeOnResize
|  Page | screencastFrameAck
|  Page | startRecordingFrames
|  Page | startScreencast
|  Page | stopRecordingFrames
|  Page | stopScreencast
| Runtime | callFunctionOn
| Runtime | enable
| Runtime | evaluate
| Runtime | getProperties

You can also download the [protocol.json][protocol].

## Building & Contributing

To build this project you will need [VS2017 Community][vs2017]. Make
sure to select the Windows 10 14393 SDK in the options.

Then run the following commands:

```bash

npm install
npm run build

```

The `.dll`s  need to be signed in order for Microsoft Edge to run them.
If you are doing any changes to the binaries, you will need to enable
`testsigning` mode in your machine following [this instructions][testsigning].

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct][coc].
For more information see the [Code of Conduct FAQ][coc-faq]
or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with
any additional questions or comments.

[cdp-protocol]: https://chromedevtools.github.io/devtools-protocol/
[coc]: https://opensource.microsoft.com/codeofconduct/
[coc-faq]: https://opensource.microsoft.com/codeofconduct/faq/
[connector-edge15]: https://github.com/sonarwhal/connector-edge15/blob/master/src/lib/connectors/connector-edge15/connector-edge15-launcher.ts
[edge-launch]: https://github.com/Microsoft/edge-diagnostics-launch
[localhost]: http://localhost:9222
[npm]: https://npmjs.com/package/edge-diagnostics-adapter
[protocol]: https://github.com/Microsoft/edge-diagnostics-adapter/blob/master/src/chromeProtocol/protocol.json
[releases]: https://github.com/Microsoft/EdgeDiagnosticsAdapter/releases
[testsigning]: https://msdn.microsoft.com/en-us/windows/hardware/drivers/install/the-testsigning-boot-configuration-option
[vs2017]: https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15
