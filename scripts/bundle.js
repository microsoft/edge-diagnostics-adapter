const platform = process.platform;
const architecture = process.arch;
const v8Version = process.versions.modules;
const package = require('../package.json');
const pkgName = package.name;
const pkgVersion = package.version;

const name = `${pkgName}-v${pkgVersion}-node-v${v8Version}-${platform}-${architecture}`;

console.log(name);
