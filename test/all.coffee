{stringify} = require '../src/js'

testDoc = {
  YAML: "YAML Ain't Markup Language"
  'What It Is': 'YAML is a human friendly data serialization'
  Object:
    description:
      """
      This object will be used to test how the serializer will behave
      in cases where quotes must be used to disambiguate scalar datatype.
      """
    tests: [
      5
      '5'
      5.555
      '5.666'
      {
        name: 'Nested obj'
        nestedArray: [
          true
          'false'
          {name: 'another nestedObject'}
        ]
      }
    ]
  date: new Date(0)
}

suite 'object serialization/deserialization', ->

  test 'stringify', ->
    rv = stringify testDoc
    console.log()
    console.log rv

