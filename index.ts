const _service = process.platform === 'win32' ? require('./build/Release/service') : {};

export enum Type {
    KERNEL_DRIVER       = 0x001,
    FILE_SYSTEM_DRIVER  = 0x002,
    ADAPTER             = 0x004,
    RECOGNIZER_DRIVER   = 0x008,
    WIN32_OWN_PROCESS   = 0x010,
    WIN32_SHARE_PROCESS = 0x020,
    INTERACTIVE_PROCESS = 0x100,
};

export enum State {
    STOPPED          = 0x1,
    START_PENDING    = 0x2,
    STOP_PENDING     = 0x3,
    RUNNING          = 0x4,
    CONTINUE_PENDING = 0x5,
    PAUSE_PENDING    = 0x6,
    PAUSED           = 0x7,
};

export enum ControlsAccepted {
    STOP                  = 0x001,
    PAUSE_CONTINUE        = 0x002,
    SHUTDOWN              = 0x004,
    PARAMCHANGE           = 0x008,
    NETBINDCHANGE         = 0x010,
    HARDWAREPROFILECHANGE = 0x020,
    POWEREVENT            = 0x040,
    SESSIONCHANGE         = 0x080,
    PRESHUTDOWN           = 0x100,
    TIMECHANGE            = 0x200,
    TRIGGEREVENT          = 0x400,
};

export enum StartType {
    BOOT_START   = 0x0,
    SYSTEM_START = 0x1,
    AUTO_START   = 0x2,
    DEMAND_START = 0x3,
    DISABLED     = 0x4,
};

export enum ErrorControl {
    IGNORE   = 0x0,
    NORMAL   = 0x1,
    SEVERE   = 0x2,
    CRITICAL = 0x3,
};

export enum ServiceFlags {
    RUNS_IN_SYSTEM_PROCESS = 0x1,
};

export enum TypeFilter {
    KERNEL_DRIVER       = Type.KERNEL_DRIVER,
    FILE_SYSTEM_DRIVER  = Type.FILE_SYSTEM_DRIVER,
    ADAPTER             = Type.ADAPTER,
    RECOGNIZER_DRIVER   = Type.RECOGNIZER_DRIVER,

    WIN32_OWN_PROCESS   = Type.WIN32_OWN_PROCESS,
    WIN32_SHARE_PROCESS = Type.WIN32_SHARE_PROCESS,

    INTERACTIVE_PROCESS = Type.INTERACTIVE_PROCESS,

    DRIVER              = KERNEL_DRIVER | FILE_SYSTEM_DRIVER | RECOGNIZER_DRIVER,
    WIN32               = WIN32_OWN_PROCESS | WIN32_SHARE_PROCESS,
    ALL                 = DRIVER | WIN32 | ADAPTER | INTERACTIVE_PROCESS,
};

export enum StateFilter {
    ACTIVE   = 0x1,
    INACTIVE = 0x2,
    ALL      = ACTIVE | INACTIVE,
};


export interface ServiceConfigBase {
    serviceType:       Type[];
    startType:         StartType;
    errorControl:      ErrorControl;
    dependencies:      string[];

    binaryPathName?:   string;
    loadOrderGroup?:   number;
    serviceStartName?: string;
    displayName?:      string;
    description?:      string;
}

export interface ServiceConfigDisplay extends ServiceConfigBase {
    tagId:             number;
}

export interface ServiceConfigCreateOptions extends Partial<ServiceConfigBase> {
    binaryPathName:    string;
    password?:         string;
}

export interface ServiceConfigChangeOptions extends Partial<ServiceConfigBase> {
    password?:         string;
}

export interface ServiceStatus {
    serviceType:      Type[];
    state:            State;
    controlsAccepted: ControlsAccepted[];
    exitCode:         number;
    checkPoint:       number;
    waitHint:         number;
    processId:        number;
    serviceFlags:     ServiceFlags[];
}

/////////////////////////////////////////////////////////////////////////////
// Service Control
/////////////////////////////////////////////////////////////////////////////

function bitmask(vals: number[]): number {
    let mask = 0;
    for (const val of vals) {
        mask |= val;
    }
    return mask;
}

function platformError(): string {
    return `Only win32 platform supported, not ${process.platform}`;
}

function assertWindows(): void {
    if (process.platform !== 'win32') {
        throw new Error(platformError());
    }
}

/** List names of registered services
 * 
 * @param type  Filter for service type (@see TypeFilter)
 * @param state Filter for service state (@see StateFilter)
 */
export function names(typeFilter: TypeFilter|TypeFilter[] = TypeFilter.ALL,
                      stateFilter: StateFilter = StateFilter.ALL): string[]
{
    assertWindows();
    typeFilter = Array.isArray(typeFilter) ? bitmask(typeFilter) : typeFilter;
    return _service.names(typeFilter, stateFilter);
}

/** Enumerate registered services
 * 
 * @param options.type  Filter for service type (@see Type)
 * @param options.state Filter for service state (@see State)
 */
export type EnumerateResult = {[index: string]: ServiceStatus & {displayName?: string}};
export function enumerate(typeFilter: TypeFilter|TypeFilter[] = TypeFilter.ALL,
                          stateFilter: StateFilter = StateFilter.ALL): EnumerateResult
{
    assertWindows();
    typeFilter = Array.isArray(typeFilter) ? bitmask(typeFilter) : typeFilter;
    return _service.enumerate(typeFilter, stateFilter);
}

/** Retrieve service configuration
 * @param name Name of service
 */
export function config(name: string): ServiceConfigDisplay {
    assertWindows();
    return _service.config(name);
}

/** Retrieve service status
 * @param name Name of service
 */
export function status(name: string): ServiceStatus {
    assertWindows();
    return _service.status(name);
}

/** Start service
 * @param name Name of service
 */
export function start(name: string): Promise<void> {
    return new Promise((resolve, reject) => {
        try {
            assertWindows();
            _service.start(name, (err?: Error) => {
                if (err) {
                    reject(err);
                } else {
                    resolve();
                }
            })
        } catch (err) {
            reject(err);
        }
    });
}

/** Stop service
 * @param name Name of service
 */
export function stop(name: string) {
    return new Promise((resolve, reject) => {
        try {
            assertWindows();
            _service.stop(name, (err?: Error) => {
                if (err) {
                    reject(err);
                } else {
                    resolve();
                }
            })
        } catch (err) {
            reject(err);
        }
    });
}

/** Enable service
 * @param name Name of service to enable
 * @param startType Desired start type for service
 */
export function enable(name: string, startType: StartType = StartType.AUTO_START) {
    assertWindows();
    return _service.change(name, {startType});
}

/** Disable service
 * @param name Name of service to disable
 */
export function disable(name: string): void {
    assertWindows();
    return _service.change(name, {startType: StartType.DISABLED});
}

/** Create new service
 * @param name   Name of new service
 * @param config Configuration of service
 */
export function create(name: string, config: ServiceConfigCreateOptions) {
    assertWindows();
    return _service.create(name, {
        ...config,
        serviceType:  bitmask(config.serviceType || [Type.WIN32_OWN_PROCESS]),
        startType:    config.startType    != undefined ? config.startType    : StartType.AUTO_START,
        errorControl: config.errorControl != undefined ? config.errorControl : ErrorControl.NORMAL,
    });
}

/** Change existing service
 * @param name   Name of service to change
 * @param config Configuration of service
 */
export function change(name: string, config: ServiceConfigChangeOptions) {
    assertWindows();
    return _service.change(name, {
        ...config,
        serviceType: config.serviceType ? bitmask(config.serviceType) : undefined,
    });
}

/** Remove service
 * @param name Name of service to remove
 */
export function remove(name: string): void {
    assertWindows();
    _service.remove(name);
}

/////////////////////////////////////////////////////////////////////////////
// Running as service
/////////////////////////////////////////////////////////////////////////////

function defaultStopCallback() {
    setImmediate(() => process.emit('SIGTERM', 'SIGTERM'));
}

/** Register with Windows Service Control Manager
 * @param name Name of service to remove
 * @param ignoreError Ignore errors
 */
export function run(name: string,
                    ignoreError: boolean = false,
                    initCallback?: (...args: any[]) => any,
                    stopCallback: () => any = defaultStopCallback): Promise<any[]|undefined> {
    return new Promise((resolve, reject) => {
        if (process.platform === 'win32') {
            _service.run(name, (err?: Error, ...args: any[]) => {
                if (!err && initCallback) {
                    initCallback(...args);
                } else if (err && !ignoreError) {
                    return reject(err);
                }
                resolve(err ? undefined : args);
            }, stopCallback);
        } else {
            if (ignoreError) {
                resolve();
            } else {
                reject(platformError());
            }
        }
    })
}
