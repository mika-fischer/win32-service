{
    'targets': [
        {
            'target_name': 'service',
            'conditions' : [
                ['OS=="win"', {
                    'include_dirs': [
                        "<!@(node -p \"require('node-addon-api').include\")",
                        "<!@(node -p \"require('napi-thread-safe-callback').include\")"
                    ],
                    'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
                    'libraries' : ['advapi32.lib', 'ws2_32.lib'],
                    'sources': [
                        'src/main.cpp',
                        'src/service.cpp',
                        'src/service-control.cpp',
                        'src/utils.cpp'
                    ],
                    'cflags!': [ '-fno-exceptions' ],
                    'cflags_cc!': [ '-fno-exceptions' ],
                    'xcode_settings': {
                        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                        'CLANG_CXX_LIBRARY': 'libc++',
                        'MACOSX_DEPLOYMENT_TARGET': '10.7',
                    },
                    'msvs_settings': {
                        'VCCLCompilerTool': { 'ExceptionHandling': 1 },
                    },
                    'conditions': [
                        ['OS=="win"', { 'defines': [ '_HAS_EXCEPTIONS=1' ] }]
                    ]
                }]
            ]
        }
    ]
}