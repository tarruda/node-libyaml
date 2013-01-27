{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'src/cpp/yaml.cpp' ],
      'dependencies': [
        'deps/yaml/yaml.gyp:yaml'
      ],
# Re-enable c++ exceptions. Taken from
# https://github.com/TooTallNate/node-gyp/issues/17
# TODO check if anything else is needed to make this compile on windows
      'cflags_cc!': [ '-fno-exceptions' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          }
        }]
      ]
    }
  ]
}
