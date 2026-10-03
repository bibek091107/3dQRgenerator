#include "Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace qrjson {

// ─────────────────────────────────────────────
//  Object / array helpers
// ─────────────────────────────────────────────

const Value* Value::Find(const std::string& key) const {
    if (!IsObject()) return nullptr;
    for (const auto& kv : m_Object) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

void Value::Set(const std::string& key, Value v) {
    if (!IsObject()) { m_Type = Type::Object; m_Object.clear(); }
    for (auto& kv : m_Object) {
        if (kv.first == key) { kv.second = std::move(v); return; }
    }
    m_Object.emplace_back(key, std::move(v));
}

std::string Value::GetString(const std::string& key, const std::string& def) const {
    const Value* v = Find(key);
    return v ? v->AsString(def) : def;
}

double Value::GetNumber(const std::string& key, double def) const {
    const Value* v = Find(key);
    return v ? v->AsNumber(def) : def;
}

int Value::GetInt(const std::string& key, int def) const {
    const Value* v = Find(key);
    if (!v || !v->IsNumber()) return def;
    return (int)std::llround(v->AsNumber());
}

bool Value::GetBool(const std::string& key, bool def) const {
    const Value* v = Find(key);
    return v ? v->AsBool(def) : def;
}

void Value::Push(Value v) {
    if (!IsArray()) { m_Type = Type::Array; m_Array.clear(); }
    m_Array.push_back(std::move(v));
}

// ─────────────────────────────────────────────
//  Serialization
// ─────────────────────────────────────────────

std::string EscapeString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

static std::string FormatNumber(double v) {
    if (std::isnan(v) || std::isinf(v)) return "null";
    if (v == static_cast<double>(static_cast<long long>(v)) &&
        std::fabs(v) < 1e15) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        return buf;
    }
    // Shortest representation that round-trips exactly.
    char buf[40];
    for (int prec = 15; prec <= 17; ++prec) {
        snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (strtod(buf, nullptr) == v) return buf;
    }
    return buf;
}

static void Newline(std::string& out, int indent, int depth) {
    if (indent < 0) return;
    out.push_back('\n');
    out.append(static_cast<size_t>(indent * depth), ' ');
}

void Value::DumpTo(std::string& out, int indent, int depth) const {
    switch (m_Type) {
        case Type::Null:   out += "null"; break;
        case Type::Bool:   out += m_Bool ? "true" : "false"; break;
        case Type::Number: out += FormatNumber(m_Number); break;
        case Type::String: out += EscapeString(m_String); break;
        case Type::Array:
            if (m_Array.empty()) { out += "[]"; break; }
            out.push_back('[');
            for (size_t i = 0; i < m_Array.size(); ++i) {
                if (i) out.push_back(',');
                Newline(out, indent, depth + 1);
                m_Array[i].DumpTo(out, indent, depth + 1);
            }
            Newline(out, indent, depth);
            out.push_back(']');
            break;
        case Type::Object:
            if (m_Object.empty()) { out += "{}"; break; }
            out.push_back('{');
            for (size_t i = 0; i < m_Object.size(); ++i) {
                if (i) out.push_back(',');
                Newline(out, indent, depth + 1);
                out += EscapeString(m_Object[i].first);
                out.push_back(':');
                if (indent >= 0) out.push_back(' ');
                m_Object[i].second.DumpTo(out, indent, depth + 1);
            }
            Newline(out, indent, depth);
            out.push_back('}');
            break;
    }
}

std::string Value::Dump(int indent) const {
    std::string out;
    DumpTo(out, indent, 0);
    return out;
}

// ─────────────────────────────────────────────
//  Parsing
// ─────────────────────────────────────────────

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : m_Text(text) {}

    Value ParseDocument() {
        SkipWs();
        Value v = ParseValue(0);
        SkipWs();
        if (m_Pos != m_Text.size()) Fail("trailing characters after JSON value");
        return v;
    }

private:
    [[noreturn]] void Fail(const std::string& msg) const {
        throw std::runtime_error("JSON parse error at offset " +
                                 std::to_string(m_Pos) + ": " + msg);
    }

    void SkipWs() {
        while (m_Pos < m_Text.size()) {
            char c = m_Text[m_Pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') m_Pos++;
            else break;
        }
    }

    char Peek() const {
        if (m_Pos >= m_Text.size()) Fail("unexpected end of input");
        return m_Text[m_Pos];
    }

    void Expect(char c) {
        if (m_Pos >= m_Text.size() || m_Text[m_Pos] != c) {
            Fail(std::string("expected '") + c + "'");
        }
        m_Pos++;
    }

    void Literal(const char* lit) {
        size_t n = strlen(lit);
        if (m_Text.compare(m_Pos, n, lit) != 0) Fail(std::string("expected ") + lit);
        m_Pos += n;
    }

    Value ParseValue(int depth) {
        if (depth > 200) Fail("maximum nesting depth exceeded");
        switch (Peek()) {
            case '{': return ParseObject(depth);
            case '[': return ParseArray(depth);
            case '"': return Value(ParseString());
            case 't': Literal("true");  return Value(true);
            case 'f': Literal("false"); return Value(false);
            case 'n': Literal("null");  return Value();
            default:  return ParseNumber();
        }
    }

    Value ParseObject(int depth) {
        Expect('{');
        Object obj;
        SkipWs();
        if (Peek() == '}') { m_Pos++; return Value(std::move(obj)); }
        for (;;) {
            SkipWs();
            if (Peek() != '"') Fail("expected object key string");
            std::string key = ParseString();
            SkipWs();
            Expect(':');
            SkipWs();
            obj.emplace_back(std::move(key), ParseValue(depth + 1));
            SkipWs();
            char c = Peek();
            if (c == ',') { m_Pos++; continue; }
            if (c == '}') { m_Pos++; break; }
            Fail("expected ',' or '}'");
        }
        return Value(std::move(obj));
    }

    Value ParseArray(int depth) {
        Expect('[');
        Array arr;
        SkipWs();
        if (Peek() == ']') { m_Pos++; return Value(std::move(arr)); }
        for (;;) {
            SkipWs();
            arr.push_back(ParseValue(depth + 1));
            SkipWs();
            char c = Peek();
            if (c == ',') { m_Pos++; continue; }
            if (c == ']') { m_Pos++; break; }
            Fail("expected ',' or ']'");
        }
        return Value(std::move(arr));
    }

    static void AppendUtf8(std::string& out, unsigned int cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    unsigned int ParseHex4() {
        if (m_Pos + 4 > m_Text.size()) Fail("truncated \\u escape");
        unsigned int v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = m_Text[m_Pos++];
            v <<= 4;
            if      (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else Fail("invalid hex digit in \\u escape");
        }
        return v;
    }

    std::string ParseString() {
        Expect('"');
        std::string out;
        for (;;) {
            if (m_Pos >= m_Text.size()) Fail("unterminated string");
            unsigned char c = static_cast<unsigned char>(m_Text[m_Pos++]);
            if (c == '"') break;
            if (c < 0x20) Fail("control character in string");
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }

            if (m_Pos >= m_Text.size()) Fail("unterminated escape");
            char e = m_Text[m_Pos++];
            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    unsigned int cp = ParseHex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF &&
                        m_Pos + 1 < m_Text.size() &&
                        m_Text[m_Pos] == '\\' && m_Text[m_Pos + 1] == 'u') {
                        m_Pos += 2;
                        unsigned int lo = ParseHex4();
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            AppendUtf8(out, cp);
                            cp = lo;
                        }
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: Fail("invalid escape sequence");
            }
        }
        return out;
    }

    Value ParseNumber() {
        size_t start = m_Pos;
        if (m_Pos < m_Text.size() && (m_Text[m_Pos] == '-' || m_Text[m_Pos] == '+')) m_Pos++;
        while (m_Pos < m_Text.size()) {
            char c = m_Text[m_Pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-') m_Pos++;
            else break;
        }
        if (start == m_Pos) Fail("invalid number");
        std::string token = m_Text.substr(start, m_Pos - start);
        char* end = nullptr;
        double v = strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0') Fail("invalid number '" + token + "'");
        return Value(v);
    }

    const std::string& m_Text;
    size_t m_Pos = 0;
};

} // namespace

Value Parse(const std::string& text) {
    Parser p(text);
    return p.ParseDocument();
}

} // namespace qrjson
