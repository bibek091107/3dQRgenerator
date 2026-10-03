#pragma once

// Minimal, dependency-free JSON value / parser / serializer.
// Used only for the qr_engine CLI request/response boundary, so the engine
// library itself stays free of external dependencies (besides zlib + qrcodegen).

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <stdexcept>

namespace qrjson {

class Value;
using Object = std::vector<std::pair<std::string, Value>>;
using Array  = std::vector<Value>;

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() : m_Type(Type::Null) {}
    Value(std::nullptr_t) : m_Type(Type::Null) {}
    Value(bool b) : m_Type(Type::Bool), m_Bool(b) {}
    Value(double n) : m_Type(Type::Number), m_Number(n) {}
    Value(int n) : m_Type(Type::Number), m_Number((double)n) {}
    Value(long n) : m_Type(Type::Number), m_Number((double)n) {}
    Value(long long n) : m_Type(Type::Number), m_Number((double)n) {}
    Value(unsigned n) : m_Type(Type::Number), m_Number((double)n) {}
    Value(const char* s) : m_Type(Type::String), m_String(s ? s : "") {}
    Value(const std::string& s) : m_Type(Type::String), m_String(s) {}
    Value(std::string&& s) : m_Type(Type::String), m_String(std::move(s)) {}
    Value(Array a) : m_Type(Type::Array), m_Array(std::move(a)) {}
    Value(Object o) : m_Type(Type::Object), m_Object(std::move(o)) {}

    static Value MakeObject() { return Value(Object{}); }
    static Value MakeArray()  { return Value(Array{});  }

    Type type() const { return m_Type; }
    bool IsNull()   const { return m_Type == Type::Null;   }
    bool IsBool()   const { return m_Type == Type::Bool;   }
    bool IsNumber() const { return m_Type == Type::Number; }
    bool IsString() const { return m_Type == Type::String; }
    bool IsArray()  const { return m_Type == Type::Array;  }
    bool IsObject() const { return m_Type == Type::Object; }

    bool AsBool(bool def = false) const { return IsBool() ? m_Bool : def; }
    double AsNumber(double def = 0.0) const { return IsNumber() ? m_Number : def; }
    const std::string& AsString(const std::string& def) const { return IsString() ? m_String : def; }
    std::string AsString(const char* def = "") const { return IsString() ? m_String : std::string(def); }

    const Array&  AsArray()  const { static Array  e; return IsArray()  ? m_Array  : e; }
    const Object& AsObject() const { static Object e; return IsObject() ? m_Object : e; }

    // Object access (linear scan — object sizes here are small and bounded)
    const Value* Find(const std::string& key) const;
    bool Has(const std::string& key) const { return Find(key) != nullptr; }
    void Set(const std::string& key, Value v);

    // Typed convenience getters with defaults
    std::string  GetString(const std::string& key, const std::string& def = "") const;
    double       GetNumber(const std::string& key, double def = 0.0) const;
    int          GetInt(const std::string& key, int def = 0) const;
    bool         GetBool(const std::string& key, bool def = false) const;

    void Push(Value v);

    std::string Dump(int indent = -1) const;   // indent < 0 => compact

private:
    void DumpTo(std::string& out, int indent, int depth) const;

    Type        m_Type;
    bool        m_Bool   = false;
    double      m_Number = 0.0;
    std::string m_String;
    Array       m_Array;
    Object      m_Object;
};

// Throws std::runtime_error with a byte offset on malformed input.
Value Parse(const std::string& text);

// Escapes a string as a JSON string literal (including surrounding quotes).
std::string EscapeString(const std::string& s);

} // namespace qrjson
