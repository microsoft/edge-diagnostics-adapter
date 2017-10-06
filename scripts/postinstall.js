const child = require('child_process');
const path = require('path');
const regedit = require('regedit');
const arch = process.arch === 'x64' ? 'x64' : 'x86';
const keys = {
    x64: 'HKCR\\Installer\\Dependencies\\,,amd64,14.0,bundle',
    x86: 'HKCR\\Installer\\Dependencies\\,,x86,14.0,bundle'
};
const key = keys[arch];

regedit.list(key, function (err, result) {
    if(result){
        console.log('VC++ 2017 seems to be installed. Skipping installation.');
        console.log('If you have any problems, please install it manually from the "redistributable" folder.');

        return;
    }

    if(err){
        console.error('Error trying to determine if VC++ 2017 Redistributable is installed. Installing it anyways...');
    }

    console.log(`Installing Microsoft Visual C++ 2017 Redistributable (${arch})`);
    const command = `${path.join(__dirname, '..', 'redistributable', `VC_redist.${arch}.exe`)} /quiet /norestart`;
    try {
        child.execSync(command);
        console.log(`VC++ 2017 Redistributable (${arch}) Installed`);
    } catch (e) {
        console.error('Error installing VC++ 2017 Redistributable, please install it manually from the "redistributable" folder');
    }
});
