const child = require('child_process');
const path = require('path');

const arch = process.arch === 'x64' ? 'x64' : 'x86';

console.log(`Installing Microsoft Visual C++ 2017 Redistributable (${arch})`);
const command = `${path.join(__dirname, '..', 'redistributable', `VC_redist.${arch}.exe`)} /quiet /norestart`;

child.execSync(command);

console.log(`Microsoft Visual C++ 2017 Redistributable (${arch}) Installed`);
