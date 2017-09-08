var {spawnSync} = require('child_process');

if (process.platform === 'win32') {
    const {error, status, signal} = spawnSync('node-gyp', process.argv.slice(2), {stdio: 'inherit', shell: true});
    if (signal != null) {
        process.emit(signal);
    } else if (status != null) {
        process.exit(status);
    } else {
        console.error(error);
        process.exit(1);
    }
}