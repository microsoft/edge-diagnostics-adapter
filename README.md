# Edge Diagnostics Adapter
[![build status](https://travis-ci.org/Microsoft/edge-diagnostics-adapter.svg?branch=master)](https://travis-ci.org/Microsoft/edge-diagnostics-adapter)

Edge Diagnostics Adapter is a protocol adapter that enables tools to debug and diagnose Edge using the [Chrome Debugging Protocol](https://chromedevtools.github.io/debugger-protocol-viewer/).

## Installation

For now we provide binaries on our [releases page](https://github.com/Microsoft/EdgeDiagnosticsAdapter/releases) and via NPM.

#### Install via NPM
```npm install edge-diagnostics-adapter```

Binary is now located in` ./node_modules/edge-diagnostics-adapter/dist/<platform>`

## Usage

```console
./<path>/EdgeDiagnosticsAdapter.exe
```

* `--help` - show available commands
* `--launch <url>` - for opening Edge with a specific URL.
* `--port <number>` - the port number to listen on. Default is 9222.
* `--killall` - kills all running Edge processes.
* `Ctrl-C` to quit. Also, the adapter can be left running as a background process.


#### Usage via node.
To simplify usage with Node we are also providing [edge-diagnostics-launch](https://github.com/Microsoft/edge-diagnostics-launch), that works a simple wrapper for the Edge Diagnostics Adapter.

#### View and inspect debuggable targets

Navigate to [localhost:9222](http://localhost:9222). You'll see a listing of all debuggable targets.

## Supported features and API

See [supported features and API](https://github.com/Microsoft/EdgeDiagnosticsAdapter/wiki/Supported-features-and-API) or download the [protocol.json](https://github.com/Microsoft/edge-diagnostics-adapter/blob/master/src/chromeProtocol/protocol.json).

## Building & Contributing
To build and contribute to this project take a gander at the wiki pages on [building](https://github.com/Microsoft/EdgeDiagnosticsAdapter/wiki/Building) and contributing (coming soon).

### Code of Conduct
This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
