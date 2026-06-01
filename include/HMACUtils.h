//+------------------------------------------------------------------+
//|                                        HMACUtils.h              |
//|                  HMAC工具类 - 签名验证                             |
//|              提供HMAC-SHA256签名生成和验证功能                      |
//+------------------------------------------------------------------+
#pragma once

#include <string>
#include <vector>

//+------------------------------------------------------------------+
//| HMAC工具类                                                        |
//+------------------------------------------------------------------+
class HMACUtils {
public:
    // HMAC-SHA256签名生成
    static std::string HMAC_SHA256(const std::string& key, const std::string& data);

    // SHA256哈希
    static std::string SHA256(const std::string& data);

    // 验证HMAC签名
    static bool VerifyHMAC(const std::string& key, const std::string& data, const std::string& signature);

    // 生成随机nonce
    static std::string GenerateNonce(int length = 16);

    // 获取当前时间戳
    static std::string GetTimestamp();

    // URL编码
    static std::string UrlEncode(const std::string& value);

    // URL解码
    static std::string UrlDecode(const std::string& value);

private:
    // 内部辅助方法
    static std::string BytesToHex(const std::vector<unsigned char>& bytes);
    static std::vector<unsigned char> HexToBytes(const std::string& hex);
    static bool IsHexChar(char c);
    static unsigned char HexCharToValue(char c);
};

//+------------------------------------------------------------------+
//| Base64工具类                                                      |
//+------------------------------------------------------------------+
class Base64Utils {
public:
    // Base64编码
    static std::string Encode(const std::string& data);
    static std::string Encode(const unsigned char* data, size_t length);

    // Base64解码
    static std::string Decode(const std::string& encoded);
    static std::vector<unsigned char> DecodeToBytes(const std::string& encoded);

    // 检查是否为有效的Base64字符串
    static bool IsValidBase64(const std::string& encoded);

private:
    static const std::string base64_chars;
    static bool IsBase64Char(unsigned char c);
};

//+------------------------------------------------------------------+
//| 字符串工具类                                                      |
//+------------------------------------------------------------------+
class StringUtils {
public:
    // 字符串分割
    static std::vector<std::string> Split(const std::string& str, const std::string& delimiter);

    // 字符串连接
    static std::string Join(const std::vector<std::string>& strings, const std::string& delimiter);

    // 去除首尾空白字符
    static std::string Trim(const std::string& str);
    static std::string TrimLeft(const std::string& str);
    static std::string TrimRight(const std::string& str);

    // 字符串替换
    static std::string Replace(const std::string& str, const std::string& from, const std::string& to);
    static std::string ReplaceAll(const std::string& str, const std::string& from, const std::string& to);

    // 大小写转换
    static std::string ToLower(const std::string& str);
    static std::string ToUpper(const std::string& str);

    // 字符串比较（忽略大小写）
    static bool EqualsIgnoreCase(const std::string& str1, const std::string& str2);

    // 检查字符串是否以指定前缀/后缀开始/结束
    static bool StartsWith(const std::string& str, const std::string& prefix);
    static bool EndsWith(const std::string& str, const std::string& suffix);

    // 格式化字符串
    static std::string Format(const char* format, ...);
};

//+------------------------------------------------------------------+
//| 时间工具类                                                        |
//+------------------------------------------------------------------+
class TimeUtils {
public:
    // 获取当前Unix时间戳
    static long long GetUnixTimestamp();

    // 获取当前毫秒时间戳
    static long long GetMillisTimestamp();

    // 时间戳转字符串
    static std::string TimestampToString(long long timestamp);

    // 字符串转时间戳
    static long long StringToTimestamp(const std::string& timestr);

    // 格式化时间字符串
    static std::string FormatTime(long long timestamp, const std::string& format = "%Y-%m-%d %H:%M:%S");

    // 解析时间字符串
    static long long ParseTime(const std::string& timestr, const std::string& format = "%Y-%m-%d %H:%M:%S");

    // 获取高精度时间（用于性能测量）
    static double GetHighResolutionTime();
};

//+------------------------------------------------------------------+
//| JSON工具类（简单实现）                                            |
//+------------------------------------------------------------------+
class JsonUtils {
public:
    // 简单的JSON值提取（适用于简单的键值对）
    static std::string GetString(const std::string& json, const std::string& key);
    static int GetInt(const std::string& json, const std::string& key);
    static double GetDouble(const std::string& json, const std::string& key);
    static bool GetBool(const std::string& json, const std::string& key);

    // JSON字符串转义
    static std::string EscapeString(const std::string& str);
    static std::string UnescapeString(const std::string& str);

    // 构建简单的JSON字符串
    static std::string BuildObject(const std::vector<std::pair<std::string, std::string>>& pairs);
    static std::string BuildArray(const std::vector<std::string>& values);

    // 验证JSON格式
    static bool IsValidJson(const std::string& json);

private:
    static std::string ExtractValue(const std::string& json, const std::string& key);
    static std::string RemoveQuotes(const std::string& str);
};