#include <v8.h>
#include <node.h>
#include <stdlib.h>
#include <yaml.h>
#include <iostream>

using namespace v8;

Local<String> emitter_error(yaml_emitter_t *emitter, yaml_event_t *event) {
  std::string msg;
  switch (emitter->error)
  {
    case YAML_MEMORY_ERROR:
      msg = "Memory error: Not enough memory for emitting";
      break;

    case YAML_WRITER_ERROR:
      msg = "Writer error: " +
        std::string(emitter->problem);
      break;
    case YAML_EMITTER_ERROR:
      msg = "Emitter error: " +
        std::string(emitter->problem);
      break;  
    default:
      msg = "Internal error";
      break;
  }
  return String::New(msg.c_str());
}


/*
 * Used to build/manage the yaml string outside the v8 heap
 */
class YamlStringBuilder : public String::ExternalStringResource {
  private:
    struct Chunk {
      Chunk *next;
      unsigned char *data;
      size_t size;
    };
    uint16_t *str;
    size_t str_len;
    size_t total_size;
    Chunk *head;
    Chunk *tail;

  public:
    YamlStringBuilder() {
      str = NULL;
      str_len = 0;
      total_size = 0;
      head = NULL;
      tail = NULL;
    }

    ~YamlStringBuilder() {
      build();
      if (str != NULL)
        free(str);
    }

    void build() {
      if (head != NULL) {
        // concatenate chunks and return the result
        str = (uint16_t *)malloc(total_size);        
        unsigned char *pos = (unsigned char *)str;
        Chunk *current = head;
        Chunk *next;
        while (current != NULL) {
          memcpy(pos, current->data, current->size);
          pos += current->size;
          next = current->next;
          // no need to keep the individual chunks
          free(current);
          current = next;
        }
        str_len = total_size / 2;
        head = NULL;
        tail = NULL;
      }
    }

    const uint16_t* data() const {
      return str;
    }

    size_t length() const {
      return str_len;
    }

    void append(unsigned char *buffer, size_t size) {
      Chunk *chunk = new Chunk();
      chunk->data = (unsigned char*)malloc(size);
      chunk->next = NULL;
      memcpy(chunk->data, buffer, size);
      chunk->size = size;
      if (head == NULL) {
        head = chunk;
        tail = chunk;
      } else {
        tail->next = chunk;
        tail = chunk;
      }
      total_size += size;
    }
};


int append_wrapper(void *b, unsigned char *buffer, size_t size) {
  YamlStringBuilder *builder = (YamlStringBuilder *)b;
  builder->append(buffer, size);
  return 1;
}

inline void stringify_object(Local<Function> scalarProcessor,
    Local<Object> obj, yaml_emitter_t *emitter,
    yaml_event_t *event, bool isArray);


inline bool stringify_scalar(Local<Function> scalarProcessor,
    Local<Value> value, yaml_emitter_t *emitter,
    yaml_event_t *event) {
  const char *tag = NULL;
  yaml_scalar_style_e style = YAML_PLAIN_SCALAR_STYLE;
  if (value->IsNull() || value->IsUndefined()) {
    tag = YAML_NULL_TAG;
  } else if (value->IsBoolean() || value->IsBooleanObject()) {
    tag = YAML_BOOL_TAG;
  } else if (value->IsInt32() || value->IsUint32() ||
      value->IsNumberObject()) {
    tag = YAML_INT_TAG;
  } else if (value->IsNumber()) {
    tag = YAML_FLOAT_TAG;
  } else if (value->IsString() || value->IsStringObject()) {
    tag = YAML_STR_TAG;
    Local<Value> args[1] = { value };
    Local<Value> quoteType = scalarProcessor->Call(
        Context::GetCurrent()->Global(),
        1, args);
    if (quoteType->IsTrue())
      style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
    else if (quoteType->IsFalse())
      style = YAML_DOUBLE_QUOTED_SCALAR_STYLE;
  } else if (value->IsDate()) {
    tag = YAML_TIMESTAMP_TAG;
    Local<Value> args[1] = { value };
    value = scalarProcessor->Call(
        Context::GetCurrent()->Global(),
        1, args);
  }
  if (tag) {
    String::Utf8Value utf8Str(value);
    if (utf8Str.length() > 50)
      style = YAML_LITERAL_SCALAR_STYLE;
    yaml_scalar_event_initialize(event, NULL,
        (yaml_char_t *)tag,
        (yaml_char_t *)*utf8Str, utf8Str.length(),
        1, 1, style);
    if (!yaml_emitter_emit(emitter, event))
      throw emitter_error(emitter, event);
    return true;
  }
  return false;
}

void stringify_value(Local<Function> scalarProcessor,
    Local<Value> value, yaml_emitter_t *emitter,
    yaml_event_t *event) {
  if (!stringify_scalar(scalarProcessor, value, emitter, event) &&
      value->IsArray()) {
    yaml_sequence_start_event_initialize(event, NULL,
        (yaml_char_t *)YAML_SEQ_TAG, 1,
        YAML_BLOCK_SEQUENCE_STYLE);
    if (!yaml_emitter_emit(emitter, event))
      throw emitter_error(emitter, event);
    stringify_object(scalarProcessor,
        value->ToObject(), emitter, event, true);
    yaml_sequence_end_event_initialize(event);
    if (!yaml_emitter_emit(emitter, event))
      throw emitter_error(emitter, event);
  } else if (value->IsObject() && !value->IsFunction() &&
      !value->IsRegExp() && !value->IsDate()) {
    yaml_mapping_start_event_initialize(event, NULL,
        (yaml_char_t *)YAML_MAP_TAG, 1,
        YAML_BLOCK_MAPPING_STYLE);
    if (!yaml_emitter_emit(emitter, event))
      throw emitter_error(emitter, event);
    stringify_object(scalarProcessor,
        value->ToObject(), emitter, event, false);
    yaml_mapping_end_event_initialize(event);
    if (!yaml_emitter_emit(emitter, event))
      throw emitter_error(emitter, event);
  }
}

inline void stringify_object(Local<Function> scalarProcessor,
    Local<Object> obj, yaml_emitter_t *emitter,
    yaml_event_t *event, bool isArray) {
  const Local<Array> props = obj->GetOwnPropertyNames();
  const uint32_t length = props->Length();
  for (uint32_t i=0 ; i<length ; ++i) {
    const Local<Value> key = props->Get(i);
    if (!isArray) {
      // emit the key
      String::Utf8Value keyStr(key);
      yaml_scalar_event_initialize(event, NULL,
          (yaml_char_t *)YAML_STR_TAG,
          (yaml_char_t *)*keyStr,
          keyStr.length(),
          1, 1, YAML_PLAIN_SCALAR_STYLE);
      if (!yaml_emitter_emit(emitter, event))
        throw emitter_error(emitter, event);

    }
    const Local<Value> child = obj->Get(key);
    stringify_value(scalarProcessor, child, emitter, event);
  }
}

inline void emit_yaml_events(Local<Function> scalarProcessor,
    Local<Value> value, yaml_emitter_t *emitter,
    yaml_event_t *event) {
  // STREAM-START/DOCUMENT-START events
  yaml_stream_start_event_initialize(event, YAML_UTF16LE_ENCODING);
  if (!yaml_emitter_emit(emitter, event))
    throw emitter_error(emitter, event);
  yaml_document_start_event_initialize(event, NULL, NULL, NULL, 1);
  if (!yaml_emitter_emit(emitter, event))
    throw emitter_error(emitter, event);
  // transverse object
  stringify_value(scalarProcessor, value, emitter, event);
  // DOCUMENT-END/STREAM-END events
  yaml_document_end_event_initialize(event, 1);
  if (!yaml_emitter_emit(emitter, event))
    throw emitter_error(emitter, event);
  yaml_stream_end_event_initialize(event);
  if (!yaml_emitter_emit(emitter, event))
    throw emitter_error(emitter, event);
  // Clear resources associated with event/emitter
  yaml_event_delete(event);
  yaml_emitter_delete(emitter);
}

// Accepts a javascript object and returns a string containing the result
Handle<Value> stringify(const Arguments& args) {
  HandleScope scope;
  YamlStringBuilder *builder = new YamlStringBuilder();
  yaml_emitter_t emitter;
  yaml_event_t event;
  yaml_emitter_initialize(&emitter);
  yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);
  yaml_emitter_set_output(&emitter, &append_wrapper, builder);
  try {
    emit_yaml_events(Local<Function>::Cast(args[1]),
        args[0], &emitter, &event);
  } catch (Local<String> msg) {
    delete builder;
    yaml_emitter_delete(&emitter);
    return ThrowException(Exception::Error(msg));
  }
  yaml_emitter_delete(&emitter);
  builder->build();
  return scope.Close(String::NewExternal(builder));
}

// Accepts a javascript string and returns the parsed object
Handle<Value> parse(const Arguments& args) {
  HandleScope scope;
  yaml_parser_t parser;



}

void init(Handle<Object> target) {
  NODE_SET_METHOD(target, "stringify", stringify);
  NODE_SET_METHOD(target, "parse", parse);
}

NODE_MODULE(binding, init);
