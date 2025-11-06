#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>

static std::string generate_uuid_v4()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist32(0, 0xffffffffu);
    auto d = [&]() { return dist32(gen); };
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::nouppercase;
    ss << std::setw(8) << (d());
    ss << "-" << std::setw(4) << ((d() >> 16) & 0xffff);
    ss << "-" << std::setw(4) << ((d() >> 16) & 0xffff);
    ss << "-" << std::setw(4) << ((d() >> 16) & 0xffff);
    ss << "-" << std::setw(12) << ((uint64_t)d() << 32 | d());
    return ss.str();
}

static std::string current_timestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

static std::string json_escape(const std::string& s)
{
    std::ostringstream o;
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if (c < 0x20)
            {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec << std::setfill(' ');
            }
            else
            {
                o << c;
            }
        }
    }
    return o.str();
}

static std::string get_appdata_devices_path()
{
    namespace fs = std::filesystem;
    fs::path dir = R"(V:\PersonalCodeBase\MiniGridMonitor)";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "devices.csv").string();
}

static void save_device_json(const std::string& path, const std::string& json_obj)
{
    namespace fs = std::filesystem;
    std::string content;
    if (fs::exists(path))
    {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        content = ss.str();
        in.close();
        auto endpos = content.find_last_not_of(" \t\r\n");
        if (endpos == std::string::npos)
            content.clear();
        else
            content.resize(endpos + 1);
    }
    std::string outContent;
    if (content.empty() || content == "[]")
        outContent = std::string("[") + json_obj + "]";
    else if (!content.empty() && content.front() == '[' && content.back() == ']')
    {
        content.pop_back();
        outContent = content + "," + json_obj + "]";
    }
    else
        outContent = std::string("[") + json_obj + "]";

    fs::path p(path);
    fs::path temp = p;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::out | std::ios::binary);
        out << outContent;
        out.close();
    }
    std::error_code ec;
    if (fs::exists(p, ec))
        fs::remove(p, ec);
    fs::rename(temp, p, ec);
    if (ec)
    {
        std::error_code ec2;
        fs::copy_file(temp, p, fs::copy_options::overwrite_existing, ec2);
        fs::remove(temp, ec2);
    }
}

int main()
{
    std::string uuid = generate_uuid_v4();
    std::string ts = current_timestamp();
    std::string obj = "{";
    obj += "\"uuid\":\"" + json_escape(uuid) + "\",";
    obj += "\"created_at\":\"" + json_escape(ts) + "\",";
    obj += "\"device_id\":\"test-id\",";
    obj += "\"device_name\":\"test-device\",";
    obj += "\"status\":\"Online\",";
    obj += "\"voltage\":\"12.3\",";
    obj += "\"temperature\":\"36.5\",";
    obj += "\"comment\":\"auto-generated test entry\"";
    obj += "}";

    std::string path = get_appdata_devices_path();
    try {
        save_device_json(path, obj);
        std::cout << "Wrote device entry to: " << path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error writing device json: " << e.what() << std::endl;
        return 2;
    }

    // Output file content for verification
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (in)
    {
        std::stringstream ss;
        ss << in.rdbuf();
        std::cout << "\nFile content:\n" << ss.str() << std::endl;
    }
    else
    {
        std::cerr << "Failed to open file after write: " << path << std::endl;
        return 3;
    }

    return 0;
}
