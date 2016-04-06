# Edge Diagnostics Adapter

Edge Diagnostics Adapter is a protocol adaptor that enables tools to debug and diagnose Edge using the Chrome Debugging Protocol.

**We are aiming to release a binary of the EdgeDiagnosticsAdapter within the next coming weeks, in addition to our Edge Debugger for VSCode.**

## Installation
*Release coming soon.*

## Running

1. Launch Edge and browse to the site you want to debug 
2. Run the EdgeDiagnosticsAdapter.exe
3. Go to `http://localhost:9222/`

## Supported features and API

| Area  | Method    |
|-------|-----------|
|  DOM  | getDocument
|  DOM  | getAttributes
|  DOM  | hideHighlight
|  DOM  | setPropertyText
|  DOM  | getStyleSheetText
|  DOM  | highlightNode
|  DOM  | setInspectModeEnabled
|  DOM  | requestChildNodes
|  DOM  | getInlineStylesForNode
|  DOM  | getMatchedStylesForNode
|  DOM  | getComputedStyleForNode
|  DOM  | pushNodesByBackendIdsToFrontend
|  DOM  | pushNodeByPathToFrontend
|  DOM  | pushNodeByPathToFrontend
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
