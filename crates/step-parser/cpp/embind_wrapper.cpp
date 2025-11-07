#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>

extern "C" {
    char* parse_step_c(const unsigned char* data, size_t len);
    void free_string(char* s);
}

std::string parseStepFile(const std::string& data) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.c_str());
    size_t len = data.length();

    char* result = parse_step_c(bytes, len);

    if (result == nullptr) {
        return "{\"error\": \"Failed to parse STEP file\"}";
    }

    std::string json(result);
    free_string(result);

    return json;
}

emscripten::val parseStepFileFromUint8Array(const emscripten::val& uint8Array) {
    unsigned int length = uint8Array["length"].as<unsigned int>();
    std::vector<unsigned char> data(length);

    emscripten::val memory = emscripten::val::module_property("HEAPU8");

    for (unsigned int i = 0; i < length; i++) {
        data[i] = uint8Array[i].as<unsigned char>();
    }

    char* result = parse_step_c(data.data(), length);

    if (result == nullptr) {
        return emscripten::val("{\"error\": \"Failed to parse STEP file\"}");
    }

    std::string json(result);
    free_string(result);

    return emscripten::val(json);
}

EMSCRIPTEN_BINDINGS(step_parser) {
    emscripten::function("parseStepFile", &parseStepFile);
    emscripten::function("parseStepFileFromUint8Array", &parseStepFileFromUint8Array);
}
