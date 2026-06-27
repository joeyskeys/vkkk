#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vkkk::built_in_shader
{

enum class ShaderStage : uint8_t {
    Vertex,
    Fragment,
    Task,
    Mesh,
    Compute
};

constexpr std::array<std::string, 5> shader_stage_strings = {
    "vertex",
    "fragment",
    "task",
    "mesh",
    "compute"
};

inline constexpr const char* to_string(ShaderStage stage) {
    return shader_stage_strings[static_cast<size_t>(stage)].c_str();
}

struct ShaderSnippet {
    std::string key;
    ShaderStage stage = ShaderStage::Vertex;
    std::string source;
};

// Stage-local build recipe. Callers decide which snippets are composed for each stage.
struct StageProgramSpec {
    ShaderStage stage = ShaderStage::Vertex;
    int glsl_version = 460;
    std::vector<std::string> extensions;
    std::vector<std::string> defines;
    std::vector<std::string> snippet_keys;
};

struct ShaderBuildPlan {
    std::vector<StageProgramSpec> stages;
};

struct GeneratedShaderPack {
    std::optional<std::string> vert;
    std::optional<std::string> frag;
    std::optional<std::string> task;
    std::optional<std::string> mesh;
    std::optional<std::string> compute;
};

// Minimal source emitter: lets callers append reusable sections and emit one GLSL unit.
class ShaderSourceEmitter {
public:
    explicit ShaderSourceEmitter(int glsl_version = 460)
        : glsl_version_(glsl_version)
    {}

    ShaderSourceEmitter& set_glsl_version(int version) {
        glsl_version_ = version;
        return *this;
    }

    ShaderSourceEmitter& add_extension(const std::string& extension_line) {
        extensions_.push_back(extension_line);
        return *this;
    }

    ShaderSourceEmitter& add_define(const std::string& define_line) {
        defines_.push_back(define_line);
        return *this;
    }

    ShaderSourceEmitter& add_global(const std::string& code) {
        globals_.push_back(code);
        return *this;
    }

    ShaderSourceEmitter& add_function(const std::string& code) {
        functions_.push_back(code);
        return *this;
    }

    ShaderSourceEmitter& add_main_statement(const std::string& code) {
        main_statements_.push_back(code);
        return *this;
    }

    std::string emit() const {
        std::ostringstream out;
        out << "#version " << glsl_version_ << "\n";
        for (const auto& ext : extensions_) {
            out << ext << "\n";
        }
        if (!extensions_.empty()) {
            out << "\n";
        }

        for (const auto& def : defines_) {
            out << def << "\n";
        }
        if (!defines_.empty()) {
            out << "\n";
        }

        for (const auto& g : globals_) {
            out << g << "\n\n";
        }
        for (const auto& fn : functions_) {
            out << fn << "\n\n";
        }

        out << "void main() {\n";
        for (const auto& stmt : main_statements_) {
            out << stmt << "\n";
        }
        out << "}\n";
        return out.str();
    }

private:
    int glsl_version_ = 460;
    std::vector<std::string> extensions_;
    std::vector<std::string> defines_;
    std::vector<std::string> globals_;
    std::vector<std::string> functions_;
    std::vector<std::string> main_statements_;
};

// Registry + orchestrator. Detailed separation policies can be layered later.
class ShaderCodegen {
public:
    void clear_snippets() {
        snippets_.clear();
    }

    void register_snippet(const ShaderSnippet& snippet) {
        snippets_[make_snippet_id(snippet.stage, snippet.key)] = snippet;
    }

    bool has_snippet(ShaderStage stage, const std::string& key) const {
        return snippets_.find(make_snippet_id(stage, key)) != snippets_.end();
    }

    const ShaderSnippet* find_snippet(ShaderStage stage, const std::string& key) const {
        const auto found = snippets_.find(make_snippet_id(stage, key));
        if (found == snippets_.end()) {
            return nullptr;
        }
        return &found->second;
    }

    std::optional<std::string> generate_stage_source(const StageProgramSpec& stage_spec) const {
        ShaderSourceEmitter emitter(stage_spec.glsl_version);
        for (const auto& ext : stage_spec.extensions) {
            emitter.add_extension(ext);
        }
        for (const auto& def : stage_spec.defines) {
            emitter.add_define(def);
        }
        for (const auto& key : stage_spec.snippet_keys) {
            const auto* snippet = find_snippet(stage_spec.stage, key);
            if (snippet == nullptr) {
                return std::nullopt;
            }
            emitter.add_global(snippet->source);
        }
        return emitter.emit();
    }

    std::optional<GeneratedShaderPack> generate(const ShaderBuildPlan& plan) const {
        GeneratedShaderPack pack{};
        for (const auto& stage_spec : plan.stages) {
            const auto source = generate_stage_source(stage_spec);
            if (!source.has_value()) {
                return std::nullopt;
            }
            set_stage(pack, stage_spec.stage, *source);
        }
        return pack;
    }

private:
    static std::string make_snippet_id(ShaderStage stage, const std::string& key) {
        return std::string(to_string(stage)) + ":" + key;
    }

    static void set_stage(GeneratedShaderPack& pack, ShaderStage stage, const std::string& source) {
        switch (stage) {
            case ShaderStage::Vertex:
                pack.vert = source;
                break;
            case ShaderStage::Fragment:
                pack.frag = source;
                break;
            case ShaderStage::Task:
                pack.task = source;
                break;
            case ShaderStage::Mesh:
                pack.mesh = source;
                break;
            case ShaderStage::Compute:
                pack.compute = source;
                break;
        }
    }

private:
    std::unordered_map<std::string, ShaderSnippet> snippets_;
};

} // namespace vkkk::built_in_shader
