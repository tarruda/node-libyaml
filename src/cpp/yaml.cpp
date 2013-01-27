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

typedef struct Chunk {
        struct Chunk *next;
        unsigned char *data;
        size_t size;
} Chunk;

typedef struct {
        Chunk *head;
        Chunk *tail;
        size_t total;
} Chunks;

int append_chunk(void *list, unsigned char *buffer, size_t size) {
        Chunks *chunks = (Chunks *)list;
        Chunk *chunk = (Chunk *)malloc(sizeof(Chunk));
        chunk->data = (unsigned char*)malloc(size);
        chunk->next = NULL;
        memcpy(chunk->data, buffer, size);
        chunk->size = size;
        if (!chunks->head) {
                chunks->head = chunk;
                chunks->tail = chunk;
        } else {
                chunks->tail->next = chunk;
                chunks->tail = chunk;
        }
        chunks->total += size;
        return 1;
}

inline void stringify_object(Local<Object> obj, yaml_emitter_t *emitter,
                yaml_event_t *event, bool isArray);


inline bool stringify_scalar(Local<Value> value, yaml_emitter_t *emitter,
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
        } else if (value->IsDate()) {
                tag = YAML_TIMESTAMP_TAG;
        }

        if (tag) {
                String::Utf8Value str(value);
                yaml_scalar_event_initialize(event, NULL,
                                (yaml_char_t *)tag,
                                (yaml_char_t *)*str, str.length(),
                                1, 1, style);
                if (!yaml_emitter_emit(emitter, event))
                        throw emitter_error(emitter, event);
                return true;
        }
        return false;
}

void stringify_value(Local<Value> value, yaml_emitter_t *emitter,
                yaml_event_t *event) {
        if (!stringify_scalar(value, emitter, event) && value->IsArray()) {
                yaml_sequence_start_event_initialize(event, NULL,
                                (yaml_char_t *)YAML_SEQ_TAG, 1,
                                YAML_BLOCK_SEQUENCE_STYLE);
                if (!yaml_emitter_emit(emitter, event))
                        throw emitter_error(emitter, event);
                stringify_object(value->ToObject(), emitter, event, true);
                yaml_sequence_end_event_initialize(event);
                if (!yaml_emitter_emit(emitter, event))
                        throw emitter_error(emitter, event);
        } else if (value->IsObject() && !value->IsFunction() &&
                        !value->IsRegExp()) {
                yaml_mapping_start_event_initialize(event, NULL,
                                (yaml_char_t *)YAML_MAP_TAG, 1,
                                YAML_BLOCK_MAPPING_STYLE);
                if (!yaml_emitter_emit(emitter, event))
                        throw emitter_error(emitter, event);
                stringify_object(value->ToObject(), emitter, event, false);
                yaml_mapping_end_event_initialize(event);
                if (!yaml_emitter_emit(emitter, event))
                        throw emitter_error(emitter, event);
        }
}

inline void stringify_object(Local<Object> obj, yaml_emitter_t *emitter,
                yaml_event_t *event, bool isArray) {
        const Local<Array> props = obj->GetPropertyNames();
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
                stringify_value(child, emitter, event);
        }
}

inline void emit_yaml_events(Local<Value> value, yaml_emitter_t *emitter,
                yaml_event_t *event) {
        // STREAM-START/DOCUMENT-START events
        yaml_stream_start_event_initialize(event, YAML_UTF8_ENCODING);
        if (!yaml_emitter_emit(emitter, event))
                throw emitter_error(emitter, event);
        yaml_document_start_event_initialize(event, NULL, NULL, NULL, 1);
        if (!yaml_emitter_emit(emitter, event))
                throw emitter_error(emitter, event);
        // transverse object
        stringify_value(value, emitter, event);
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
        Chunks chunks = { NULL, NULL, 0 };
        yaml_emitter_t emitter;
        yaml_event_t event;
        yaml_emitter_initialize(&emitter);
        yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
        yaml_emitter_set_output(&emitter, &append_chunk, &chunks);
        try {
                emit_yaml_events(args[0], &emitter, &event);
        } catch (Local<String> msg) {
                yaml_emitter_delete(&emitter);
                return ThrowException(Exception::Error(msg));
        }
        // concatenate chunks that will be copied into a v8 string
        unsigned char *temp = (unsigned char*)malloc(chunks.total);        
        unsigned char *pos = temp;
        Chunk *current = chunks.head;
        Chunk *next;
        while (current != NULL) {
                memcpy(pos, current->data, current->size);
                pos += current->size;
                next = current->next;
                free(current);
                current = next;
        }
        assert(chunks.total == (size_t)(pos - temp));
        Local<String> rv = String::New((const char*)temp, chunks.total);
        free(temp);
        yaml_emitter_delete(&emitter);
        return scope.Close(rv);
}

// Accepts a javascript string and returns the parsed object
Handle<Value> parse(const Arguments& args) {
        HandleScope scope;

}

void init(Handle<Object> target) {
        NODE_SET_METHOD(target, "stringify", stringify);
        NODE_SET_METHOD(target, "parse", parse);
}

NODE_MODULE(binding, init);
