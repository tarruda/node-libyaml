{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {},
      'Release': {}
    }
  },

  'targets': [
    {
      'target_name': 'yaml',
      'type': 'static_library',
      'include_dirs': [ 'include' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ]
      },
      'defines': [
        'YAML_VERSION_MAJOR=0',
        'YAML_VERSION_MINOR=1',
        'YAML_VERSION_PATCH=4',
        'YAML_VERSION_STRING="0.1.4"'
      ],
      'sources': [
        'src/api.c',
        'src/reader.c',
        'src/scanner.c',
        'src/parser.c',
        'src/loader.c',
        'src/writer.c',
        'src/emitter.c',
        'src/dumper.c'
      ]
    }
  ]
}
