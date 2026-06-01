//+------------------------------------------------------------------+
//|                                        HMACUtils.cpp            |
//|                  HMAC工具类 - 实现文件                             |
//+------------------------------------------------------------------+
#include "HMACUtils.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <random>
#include <chrono>
#include <cstdarg>
#include <algorithm>
#include <cctype>

//+------------------------------------------------------------------+
//| HMAC工具类实现                                                    |
//+------------------------------------------------------------------+
std::string HMACUtils::HMAC_SHA256(const std::string& key, const std::string& data) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    HMAC(EVP_sha256(),
         key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         result, &result_len);

    std::vector<unsigned char> hash_bytes(result, result + result_len);
    return BytesToHex(hash_bytes);
}

std::string HMACUtils::SHA256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.length());
    SHA256_Final(hash, &sha256);

    std::vector<unsigned char> hash_bytes(hash, hash + SHA256_DIGEST_LENGTH);
    return BytesToHex(hash_bytes);
}

bool HMACUtils::VerifyHMAC(const std::string& key, const std::string& data, const std::string& signature) {
    std::string computed_signature = HMAC_SHA256(key, data);
    return computed_signature == signature;
}

std::string HMACUtils::GenerateNonce(int length) {
    const std::string chars = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string nonce;
    nonce.reserve(length);

    for (int i = 0; i < length; ++i) {
        nonce += chars[dis(gen)];
    }

    return nonce;
}

std::string HMACUtils::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

std::string HMACUtils::UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

std::string HMACUtils::UrlDecode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.length());

    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%' && i + 2 < value.length()) {
            if (IsHexChar(value[i + 1]) && IsHexChar(value[i + 2])) {
                unsigned char byte = (HexCharToValue(value[i + 1]) << 4) | HexCharToValue(value[i + 2]);
                decoded += static_cast<char>(byte);
                i += 2;
            } else {
                decoded += value[i];
            }
        } else if (value[i] == '+') {
            decoded += ' ';
        } else {
            decoded += value[i];
        }
    }

    return decoded;
}

std::string HMACUtils::BytesToHex(const std::vector<unsigned char>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (unsigned char byte : bytes) {
        oss << std::setw(2) << static_cast<int>(byte);
    }

    return oss.str();
}

std::vector<unsigned char> HMACUtils::HexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length() && IsHexChar(hex[i]) && IsHexChar(hex[i + 1])) {
            unsigned char byte = (HexCharToValue(hex[i]) << 4) | HexCharToValue(hex[i + 1]);
            bytes.push_back(byte);
        }
    }

    return bytes;
}

bool HMACUtils::IsHexChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

unsigned char HMACUtils::HexCharToValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

//+------------------------------------------------------------------+
//| Base64工具类实现                                                  |
//+------------------------------------------------------------------+
const std::string Base64Utils::base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string Base64Utils::Encode(const std::string& data) {
    return Encode(reinterpret_cast<const unsigned char*>(data.c_str()), data.length());
}

std::string Base64Utils::Encode(const unsigned char* data, size_t length) {
    std::string encoded;
    int val = 0, valb = -6;

    for (size_t i = 0; i < length; ++i) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (encoded.size() % 4) {
        encoded.push_back('=');
    }

    return encoded;
}

std::string Base64Utils::Decode(const std::string& encoded) {
    auto bytes = DecodeToBytes(encoded);
    return std::string(bytes.begin(), bytes.end());
}

std::vector<unsigned char> Base64Utils::DecodeToBytes(const std::string& encoded) {
    std::vector<unsigned char> decoded;
    int val = 0, valb = -8;

    for (char c : encoded) {
        if (!IsBase64Char(c)) break;

        val = (val << 6) + base64_chars.find(c);
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return decoded;
}

bool Base64Utils::IsValidBase64(const std::string& encoded) {
    if (encoded.length() % 4 != 0) return false;

    for (size_t i = 0; i < encoded.length(); ++i) {
        char c = encoded[i];
        if (i >= encoded.length() - 2 && c == '=') continue;
        if (!IsBase64Char(c)) return false;
    }

    return true;
}

bool Base64Utils::IsBase64Char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/';
}

//+------------------------------------------------------------------+
//| 字符串工具类实现                                                  |
//+------------------------------------------------------------------+
std::vector<std::string> StringUtils::Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
}

std::string StringUtils::Join(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) return "";

    std::ostringstream oss;
    oss << strings[0];

    for (size_t i = 1; i < strings.size(); ++i) {
        oss << delimiter << strings[i];
    }

    return oss.str();
}

std::string StringUtils::Trim(const std::string& str) {
    return TrimLeft(TrimRight(str));
}

std::string StringUtils::TrimLeft(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    return (start == std::string::npos) ? "" : str.substr(start);
}

std::string StringUtils::TrimRight(const std::string& str) {
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

std::string StringUtils::Replace(const std::string& str, const std::string& from, const std::string& to) {
    size_t pos = str.find(from);
    if (pos == std::string::npos) return str;

    std::string result = str;
    result.replace(pos, from.length(), to);
    return result;
}

std::string StringUtils::ReplaceAll(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t pos = 0;

    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }

    return result;
}

std::string StringUtils::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string StringUtils::ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

bool StringUtils::EqualsIgnoreCase(const std::string& str1, const std::string& str2) {
    return ToLower(str1) == ToLower(str2);
}

bool StringUtils::StartsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() &&
           str.compare(0, prefix.length(), prefix) == 0;
}

bool StringUtils::EndsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() &&
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string StringUtils::Format(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // 计算所需缓冲区大小
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (size <= 0) {
        va_end(args);
        return "";
    }

    // 格式化字符串
    std::vector<char> buffer(size + 1);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);

    return std::string(buffer.data(), size);
}

//+------------------------------------------------------------------+
//| 时间工具类实现                                                    |
//+------------------------------------------------------------------+
long long TimeUtils::GetUnixTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

long long TimeUtils::GetMillisTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string TimeUtils::TimestampToString(long long timestamp) {
    return std::to_string(timestamp);
}

long long TimeUtils::StringToTimestamp(const std::string& timestr) {
    try {
        return std::stoll(timestr);
    } catch (const std::exception&) {
        return 0;
    }
}

std::string TimeUtils::FormatTime(long long timestamp, const std::string& format) {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm_info = std::localtime(&time);

    char buffer[128];
    std::strftime(buffer, sizeof(buffer), format.c_str(), tm_info);

    return std::string(buffer);
}

long long TimeUtils::ParseTime(const std::string& timestr, const std::string& format) {
    std::tm tm_info = {};
    std::istringstream ss(timestr);
    ss >> std::get_time(&tm_info, format.c_str());

    if (ss.fail()) return 0;

    return static_cast<long long>(std::mktime(&tm_info));
}

double TimeUtils::GetHighResolutionTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(duration).count();
}

//+------------------------------------------------------------------+
//| JSON工具类实现                                                    |
//+------------------------------------------------------------------+
std::string JsonUtils::GetString(const std::string& json, const std::string& key) {
    std::string value = ExtractValue(json, key);
    return RemoveQuotes(value);
}

int JsonUtils::GetInt(const std::string& json, const std::string& key) {
    std::string value = ExtractValue(json, key);
    try {
        return std::stoi(RemoveQuotes(value));
    } catch (const std::exception&) {
        return 0;
    }
}

double JsonUtils::GetDouble(const std::string& json, const std::string& key) {
    std::string value = ExtractValue(json, key);
    try {
        return std::stod(RemoveQuotes(value));
    } catch (const std::exception&) {
        return 0.0;
    }
}

bool JsonUtils::GetBool(const std::string& json, const std::string& key) {
    std::string value = StringUtils::ToLower(RemoveQuotes(ExtractValue(json, key)));
    return value == "true" || value == "1";
}

std::string JsonUtils::EscapeString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length() * 2);

    for (char c : str) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }

    return escaped;
}

std::string JsonUtils::UnescapeString(const std::string& str) {
    std::string unescaped;
    unescaped.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case '"': unescaped += '"'; ++i; break;
                case '\\': unescaped += '\\'; ++i; break;
                case 'b': unescaped += '\b'; ++i; break;
                case 'f': unescaped += '\f'; ++i; break;
                case 'n': unescaped += '\n'; ++i; break;
                case 'r': unescaped += '\r'; ++i; break;
                case 't': unescaped += '\t'; ++i; break;
                default: unescaped += str[i]; break;
            }
        } else {
            unescaped += str[i];
        }
    }

    return unescaped;
}

std::string JsonUtils::BuildObject(const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::ostringstream oss;
    oss << "{";

    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << EscapeString(pairs[i].first) << "\":\"" << EscapeString(pairs[i].second) << "\"";
    }

    oss << "}";
    return oss.str();
}

std::string JsonUtils::BuildArray(const std::vector<std::string>& values) {
    std::ostringstream oss;
    oss << "[";

    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << EscapeString(values[i]) << "\"";
    }

    oss << "]";
    return oss.str();
}

bool JsonUtils::IsValidJson(const std::string& json) {
    // 简单的JSON格式验证
    std::string trimmed = StringUtils::Trim(json);
    if (trimmed.empty()) return false;

    return (trimmed.front() == '{' && trimmed.back() == '}') ||
           (trimmed.front() == '[' && trimmed.back() == ']');
}

std::string JsonUtils::ExtractValue(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);

    if (key_pos == std::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) return "";

    size_t value_start = colon_pos + 1;
    while (value_start < json.length() && std::isspace(json[value_start])) {
        ++value_start;
    }

    if (value_start >= json.length()) return "";

    size_t value_end;
    if (json[value_start] == '"') {
        // 字符串值
        value_end = json.find('"', value_start + 1);
        if (value_end == std::string::npos) return "";
        return json.substr(value_start, value_end - value_start + 1);
    } else {
        // 数字或布尔值
        value_end = json.find_first_of(",}", value_start);
        if (value_end == std::string::npos) value_end = json.length();
        return StringUtils::Trim(json.substr(value_start, value_end - value_start));
    }
}

std::string JsonUtils::RemoveQuotes(const std::string& str) {
    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}