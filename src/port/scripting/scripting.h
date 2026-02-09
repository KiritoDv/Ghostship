#pragma once

#ifdef __cplusplus
#include <string>
#include "port/importer/ResourceType.h"
#include "port/importer/types/Text.h"

struct lua_State;

class ScriptingLayer {
  public:
    static ScriptingLayer* Instance;

    void Init();
    void Load(std::string file);
    void Load(const std::string& path, uint32_t bindings, const std::shared_ptr<Ship::Archive>& archive);
    void Clean();
    void Reload();
    static int Require(lua_State* L);
};
extern "C" {
#endif

void BindEvent(const char* name, int32_t id);

#ifdef __cplusplus
}
#endif