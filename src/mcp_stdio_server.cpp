#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <ctime>
#include <fstream>
#include <filesystem>

#include "prompt_slover.h"
#include "diffusion_slover.h"
#include "decoder_slover.h"

#include "stb_image_write.h"
#include "json.hpp"

using json = nlohmann::json;

static bool g_verbose = false;

static void log_stderr(const std::string& s)
{
    if (!g_verbose)
        return;
    std::cerr << s << std::endl;
}

static std::string get_string_or(const json& obj, const char* key, const std::string& def)
{
    if (!obj.is_object())
        return def;
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_string())
        return def;
    return it->get<std::string>();
}

static int get_int_or(const json& obj, const char* key, int def)
{
    if (!obj.is_object())
        return def;
    auto it = obj.find(key);
    if (it == obj.end())
        return def;
    if (it->is_number_integer())
        return it->get<int>();
    if (it->is_number_float())
        return static_cast<int>(it->get<double>());
    return def;
}

static std::string base64_encode(const unsigned char* data, size_t len)
{
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < len)
    {
        unsigned int v = (data[i] << 16) | (data[i + 1] << 8) | (data[i + 2]);
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back(tbl[(v >> 6) & 63]);
        out.push_back(tbl[v & 63]);
        i += 3;
    }

    if (i < len)
    {
        unsigned int v = data[i] << 16;
        out.push_back(tbl[(v >> 18) & 63]);
        if (i + 1 < len)
        {
            v |= data[i + 1] << 8;
            out.push_back(tbl[(v >> 12) & 63]);
            out.push_back(tbl[(v >> 6) & 63]);
            out.push_back('=');
        }
        else
        {
            out.push_back(tbl[(v >> 12) & 63]);
            out.push_back('=');
            out.push_back('=');
        }
    }

    return out;
}

static void stb_write_to_vector(void* context, void* data, int size)
{
    auto* buf = reinterpret_cast<std::vector<unsigned char>*>(context);
    unsigned char* bytes = reinterpret_cast<unsigned char*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

static std::string default_out_path(int seed, int width, int height)
{
    const std::time_t t = std::time(nullptr);
    return "mcp_outputs/result_" + std::to_string(seed) + "_" + std::to_string(width) + "x" + std::to_string(height) + "_" + std::to_string((long long)t) + ".png";
}

static bool write_bytes_file(const std::string& path, const std::vector<unsigned char>& bytes, std::string* err)
{
    try
    {
        std::filesystem::path p(path);
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path());
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            if (err) *err = "failed to open: " + path;
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out)
        {
            if (err) *err = "failed to write: " + path;
            return false;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        if (err) *err = e.what();
        return false;
    }
}

struct PipelineKey
{
    std::string assets_dir;
    int height = 256;
    int width = 256;
    int mode = 0;

    bool operator==(const PipelineKey& other) const
    {
        return assets_dir == other.assets_dir && height == other.height && width == other.width && mode == other.mode;
    }
};

class Pipeline
{
public:
    void ensure(const PipelineKey& key)
    {
        if (key_ && *key_ == key)
            return;

        log_stderr("[mcp] loading models from assets_dir=" + key.assets_dir);
        prompt_ = std::make_unique<PromptSlover>(key.assets_dir);
        diffusion_ = std::make_unique<DiffusionSlover>(key.height, key.width, key.mode, key.assets_dir);
        decoder_ = std::make_unique<DecodeSlover>(key.height, key.width, key.assets_dir);
        key_ = key;
    }

    std::vector<unsigned char> txt2img_png(const PipelineKey& key, const std::string& prompt, const std::string& negative_prompt, int steps, int seed)
    {
        ensure(key);

        std::string p = prompt;
        std::string np = negative_prompt;
        ncnn::Mat cond = prompt_->get_conditioning(p);
        ncnn::Mat uncond = prompt_->get_conditioning(np);
        ncnn::Mat sample = diffusion_->sampler(seed, steps, cond, uncond);
        ncnn::Mat decoded = decoder_->decode(sample);

        std::vector<unsigned char> rgb(key.height * key.width * 3);
        decoded.to_pixels(rgb.data(), ncnn::Mat::PIXEL_RGB);

        std::vector<unsigned char> png;
        stbi_write_png_to_func(stb_write_to_vector, &png, key.width, key.height, 3, rgb.data(), key.width * 3);
        return png;
    }

private:
    std::optional<PipelineKey> key_;
    std::unique_ptr<PromptSlover> prompt_;
    std::unique_ptr<DiffusionSlover> diffusion_;
    std::unique_ptr<DecodeSlover> decoder_;
};

static json make_result(const json& id, const json& result)
{
    return json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result},
    };
}

static json make_error(const json& id, int code, const std::string& message)
{
    return json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}},
    };
}

static bool is_valid_size(int v)
{
    return v == 256 || v == 512;
}

int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string assets_dir = "assets";
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--verbose")
        {
            g_verbose = true;
            continue;
        }
        if ((arg == "--assets" || arg == "--assets-dir") && i + 1 < argc)
        {
            assets_dir = argv[++i];
        }
    }

    Pipeline pipeline;
    std::string protocol_version = "2024-11-05";

    for (std::string line; std::getline(std::cin, line);)
    {
        if (line.empty())
            continue;

        json msg = json::parse(line, nullptr, false);
        if (msg.is_discarded() || !msg.is_object())
        {
            log_stderr("[mcp] failed to parse message: " + line);
            continue;
        }

        const json id = msg.contains("id") ? msg["id"] : json();
        const std::string method = get_string_or(msg, "method", "");
        const json params = msg.contains("params") ? msg["params"] : json::object();

        if (!msg.contains("id"))
        {
            // notification
            if (method == "exit")
                break;
            continue;
        }

        try
        {
            if (method == "initialize")
            {
                if (params.is_object() && params.contains("protocolVersion") && params["protocolVersion"].is_string())
                    protocol_version = params["protocolVersion"].get<std::string>();

                json result = {
                    {"protocolVersion", protocol_version},
                    {"capabilities", {{"tools", json::object()}}},
                    {"serverInfo", {{"name", "ncnn-llm-mcp-sdimggen"}, {"version", "0.1.0"}}},
                };
                std::cout << make_result(id, result).dump() << "\n" << std::flush;
            }
            else if (method == "tools/list")
            {
                json schema = {
                    {"type", "object"},
                    {"properties",
                     {
                         {"prompt", {{"type", "string"}, {"description", "Positive prompt"}}},
                         {"negative_prompt", {{"type", "string"}, {"description", "Negative prompt"}, {"default", ""}}},
                         {"width", {{"type", "integer"}, {"enum", {256, 512}}, {"default", 256}}},
                         {"height", {{"type", "integer"}, {"enum", {256, 512}}, {"default", 256}}},
                         {"steps", {{"type", "integer"}, {"minimum", 1}, {"maximum", 50}, {"default", 15}}},
                         {"seed", {{"type", "integer"}, {"default", 0}}},
                         {"mode", {{"type", "integer"}, {"enum", {0, 1}}, {"default", 0}}},
                         {"assets_dir", {{"type", "string"}, {"default", assets_dir}}},
                         {"output", {{"type", "string"}, {"enum", {"base64", "file", "both"}}, {"default", "base64"}}},
                         {"out_path", {{"type", "string"}, {"description", "When output includes file: write png to this path (optional)"}}},
                     }},
                    {"required", {"prompt"}},
                };

                json tool = {
                    {"name", "sd_txt2img"},
                    {"description", "Stable Diffusion (ncnn) text-to-image. Returns image/png as base64."},
                    {"inputSchema", schema},
                };

                json result = {{"tools", json::array({tool})}};
                std::cout << make_result(id, result).dump() << "\n" << std::flush;
            }
            else if (method == "tools/call")
            {
                if (!params.is_object())
                {
                    std::cout << make_error(id, -32602, "params must be an object").dump() << "\n" << std::flush;
                    continue;
                }

                const std::string name = get_string_or(params, "name", "");
                const json arguments = params.contains("arguments") ? params["arguments"] : json::object();
                if (name != "sd_txt2img")
                {
                    std::cout << make_error(id, -32601, "unknown tool: " + name).dump() << "\n" << std::flush;
                    continue;
                }
                if (!arguments.is_object())
                {
                    std::cout << make_error(id, -32602, "arguments must be an object").dump() << "\n" << std::flush;
                    continue;
                }

                const std::string prompt = get_string_or(arguments, "prompt", "");
                if (prompt.empty())
                {
                    std::cout << make_error(id, -32602, "prompt is required").dump() << "\n" << std::flush;
                    continue;
                }

                const std::string output = get_string_or(arguments, "output", "base64");
                const bool want_base64 = (output == "base64" || output == "both");
                const bool want_file = (output == "file" || output == "both");
                if (output != "base64" && output != "file" && output != "both")
                {
                    std::cout << make_error(id, -32602, "output must be one of: base64, file, both").dump() << "\n" << std::flush;
                    continue;
                }

                PipelineKey key;
                key.assets_dir = get_string_or(arguments, "assets_dir", assets_dir);
                key.height = get_int_or(arguments, "height", 256);
                key.width = get_int_or(arguments, "width", 256);
                key.mode = get_int_or(arguments, "mode", 0);

                if (!is_valid_size(key.height) || !is_valid_size(key.width))
                {
                    std::cout << make_error(id, -32602, "height/width only support 256 or 512 currently").dump() << "\n" << std::flush;
                    continue;
                }

                const int steps = get_int_or(arguments, "steps", 15);
                int seed = get_int_or(arguments, "seed", 0);
                if (seed == 0)
                    seed = static_cast<int>(time(nullptr));

                const std::string negative_prompt = get_string_or(arguments, "negative_prompt", "");

                std::vector<unsigned char> png = pipeline.txt2img_png(key, prompt, negative_prompt, steps, seed);

                json content = json::array();

                std::string saved_path;
                if (want_file)
                {
                    saved_path = get_string_or(arguments, "out_path", "");
                    if (saved_path.empty())
                        saved_path = default_out_path(seed, key.width, key.height);

                    std::string err;
                    if (!write_bytes_file(saved_path, png, &err))
                    {
                        std::cout << make_error(id, -32000, "failed to write png: " + err).dump() << "\n" << std::flush;
                        continue;
                    }
                    content.push_back(json{{"type", "text"}, {"text", saved_path}});
                }

                if (want_base64)
                {
                    std::string b64 = base64_encode(png.data(), png.size());
                    content.push_back(json{{"type", "image"}, {"mimeType", "image/png"}, {"data", b64}});
                }

                json result = {{"content", content}};
                if (!saved_path.empty())
                    result["outputPath"] = saved_path;
                std::cout << make_result(id, result).dump() << "\n" << std::flush;
            }
            else if (method == "shutdown")
            {
                std::cout << make_result(id, nullptr).dump() << "\n" << std::flush;
            }
            else
            {
                std::cout << make_error(id, -32601, "method not found: " + method).dump() << "\n" << std::flush;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << make_error(id, -32000, e.what()).dump() << "\n" << std::flush;
        }
    }

    return 0;
}
