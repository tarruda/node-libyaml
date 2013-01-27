{stringify} = require '../src/js'


suite 'object serialization/deserialization', ->

  test 'test', ->
    rv = stringify [
      {objects:[new Date(), {name: 'thiago'},'2',3.2,true,5,6], name2:'SADSASDA\n\nSADSDSA'}
    ]
    console.log rv

