const fs = require('fs');
const path = require('path');
const util = require('util');

const service = require("./index");

const name = 'ABCDEF';
const displayName = 'ABCDEF Service';

async function enumerate() {
    const result = service.enumerate(service.TypeFilter.WIN32, service.StateFilter.ACTIVE);
    console.log(result);
}

async function names() {
    const result = service.names(service.TypeFilter.WIN32, service.StateFilter.ACTIVE);
    console.log(result);
}

async function create () {
    service.create(name, {
        displayName,
        binaryPathName: [process.execPath, ...process.execArgv, __filename].join(' '),
    });
}

async function remove() {
    service.remove(name);
}

async function enable() {
    service.enable(name);
}

async function disable() {
    service.disable(name);
}

async function status() {
    console.log({
        config: service.config(name),
        status: service.status(name),
    });
}

async function start() {
    await service.start(name);
}

async function stop() {
    await service.stop(name);
}

async function restart() {
    console.log(service.status(name).state);
    await service.stop(name);
    console.log(service.status(name).state);
    await service.start(name);
    console.log(service.status(name).state);
}

async function run() {
    const logFile = fs.createWriteStream(path.join(__dirname, 'log.txt'), {flags: 'w'});
    console.log = function () {
        logFile.write(util.format.apply(null, arguments) + '\n');
        process.stdout.write(util.format.apply(null, arguments) + '\n');
    }
    console.error = console.log;

    console.log('Trying to run as service');
    const args = await service.run(name);
    console.log('Args:', args);

    const interval = setInterval(() => console.log('Still running'), 30000);
    for (const sig of ['SIGTERM', 'SIGINT', 'SIGHUP', 'SIGBREAK']) {
        process.on(sig, () => {
            clearInterval(interval);
            console.log('Received signal', sig);
            console.log('Shutting down');
            setTimeout(() => {
                console.log('Node not exiting by itself after 10000 ms, exiting forcefully');
                setImmediate(() => process.exit(0));
            }, 10000).unref();
        });
    }

}

async function main() {
    const args = process.argv.slice(2);
    switch (args[0]) {
    case 'enumerate':
        return await enumerate();
    case 'names':
        return await names();
    case 'create':
        return await create();
    case 'remove':
        return await remove();
    case 'status':
        return await status();
    case 'start':
        return await start();
    case 'stop':
        return await stop();
    case 'restart':
        return await restart();
    case 'enable':
        return await enable();
    case 'disable':
        return await disable();
    default:
        return await run();
    }
};

main().catch(console.error);