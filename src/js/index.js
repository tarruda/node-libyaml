var _yaml = require('../../build/Release/binding.node')
  , util = require('util')
  // Regexes taken from http://yaml.org/type/
  , NULL = '(?:null)'
  , BOOL = '(?:y|yes|n|no|true|false|on|off)'
  , INT2 = '(?:[-+]?0b[0-1_]+)'
  , INT8 = '(?:[-+]?0[0-7_]+)'
  , INT10 = '(?:[-+]?(?:0|[1-9][0-9_]*))'
  , INT16 = '(?:[-+]?0x[0-9a-f_]+)'
  , INT60 = '(?:[-+]?[1-9][0-9_]*(?::[0-5]?[0-9])+)'
  , FLOAT10 = '(?:[-+]?(?:[0-9][0-9_]*)?\\.[0-9.]*(?:[e][-+][0-9]+)?)'
  , FLOAT60 = '(?:[-+]?[0-9][0-9_]*(?::[0-5]?[0-9])+\\.[0-9_]*)'
  , INF = '(?:[-+]?\\.inf)'
  , NAN = '(?:\\.nan)'
  , TS1 = '(?:[0-9]{4}-[0-9]{2}-[0-9]{2})' // year-month-day
  , TS2 = '(?:[0-9]{4}-[0-9]{1,2}-[0-9]{1,2}' // year-month-day
        + '(?:[Tt]|[\\s\\t]+){1,2}' // hour
        + ':[0-9]{1,2}' // minute
        + ':[0-9]{1,2}' // second
        + '(?:\\.[0-9]*)?' // fraction
        + '(?:(?:[\\s\\t]*)Z|[-+][0-9][0-9]?(?::[0-9][0-9])?)?)'; // timezone

var yamlLiteral = new RegExp('^\\s*(?:' + [
  NULL,
  BOOL,
  INT2, INT8, INT10, INT16, INT60,
  FLOAT10, FLOAT60, INF, NAN, TS1, TS2
].join('|') + ')\\s*$', 'i');
        
function scalarProcessor(value) {
  if (typeof value === 'string' && yamlLiteral.test(value)) {
      return true;
  } else if (util.isDate(value)) {
    return value.toISOString();
  }
}

exports.stringify = function(obj) {
  return _yaml.stringify(obj, scalarProcessor)
};
