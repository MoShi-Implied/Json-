#include "tiny_json.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <utility>

namespace json11{
    // Json数据的最大深度
    static const int max_depth = 200;

    using std::string;
    using std::vector;
    using std::map;
    using std::make_shared;
    using std::move;

    /*
     * 实现一个空值的辅助结构
     * 实现了一个空结构体，并实现比较运算符
     * 这是因为C++中nullptr_t不支持比较运算
     * 这样能对Json数据进行更为灵活的处理
     */
    struct NullStruct{
        // 说明任意两个Null是相等的
        bool operator==(NullStruct) const{ return true; }
        // Null无法进行大小比较
        bool operator< (NullStruct) const{ return false; }
    };


    /*
     * 序列化部分
     */
    static void dump(NullStruct, string& out){
        out += "null";
    }

    static void dump(double value, string& out){
        // 检测value是否是一个有限数（既不是infinity（无穷大）或者NaN（非数））
        if(std::isfinite(value)){
            // 使用stringstream效率会更低
            char buf[32];
            snprintf(buf, sizeof(buf), "%.17g", value);
            out += buf;
        }
        else{
            // Json是不是对于无穷大和非数也没法处理
            out += "null";
        }
    }

    static void dump(int value, string& out){
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", value);
        out += buf;
    }

    static void dump(bool value, string& out){
        out += value ? "true" : "false";
    }

    static void dump(const string& value, string& out){
        out += '"';
        for(size_t i = 0; i < value.size(); i++){
            const char ch = value[i];
            // ASCII码
            if(ch == '\\'){
                out += "\\\\";
            }
            else if(ch == '"'){
                out += "\\\"";
            }else if(ch == '\b'){
                out += "\\b";
            }
            else if(ch == '\f'){
                out += "\\f";
            }
            else if(ch == '\n'){
                out += "\\n";
            }
            else if(ch == '\r'){
                out += "\\r";
            }
            else if('\t'){
                out += "\\t";
            }
            // 对unicode字符的支持
            else if(static_cast<uint8_t>(ch) <= 0x1f){
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", ch);
                out += buf;
            }
            else if(static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(value[i+1] == 0x80)
            && static_cast<uint8_t>(value[i+2]) == 0xa8){
                out += "\\u2028";
                i += 2;
            }
            else if(static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(value[i+1]) == 0x80
                    && static_cast<uint8_t>(value[i+2]) == 0xa9){
                out += "\\u2029";
                i += 2;
            }
            else{
                out += ch;
            }
        }
        out += '"';
    }

    // 对Json::array进行处理
    static void dump(const Json::array& values, string& out){
        bool first = true;
        out += "[";
        for(const auto& value : values){
            if(!first){
                out += ", ";
            }
            value.dump(out);
            first = false;
        }
        out += "]";
    }

    static void dump(const Json::object& values, string& out){
        bool first = true;
        out += "{";
        for(const auto& kv : values){
            if(!first){
                out += ",";
            }
            dump(kv.first, out);
            out += ": ";
            kv.second.dump(out);
            first = false;
        }
        out += "}";
    }

    void Json::dump(std::string &out) const {
        m_ptr->dump(out);
    }

    /*
     * 值包装器？？
     * 装饰类？
     */
    template<Json::Type tag, typename T>
    class Value : public JsonValue{
    protected:
        // 构造函数
        explicit Value(const T& value)  : m_value(value) {}
        /*
         * 需要去看看move
         */
        explicit Value(T&& value)       : m_value(move(value)) {}

        /*
         * override可以帮助编译器检查
         * 检查子类中的函数是否真正在重写基类中的函数
         */
        Json::Type type() const override{
            return tag;
        }

        /*
         * 比较器
         */
        bool equals(const JsonValue* other) const override{
            return m_value == static_cast<const Value<tag, T> *>(other)->m_value;
//            return m_value == static_cast<const Value<tag, T> *>(other)->m_value;
        }
        bool less(const JsonValue* other) const override{
            return m_value < static_cast<const Value<tag, T> *>(other)->m_value;
        }
        const T m_value;
        void dump(string& out) const override{
            json11::dump(m_value, out);
        }
    };

    /*
     * 具体的JsonValue类
     * 使用final表示这是一个最终类
     * 不可再被继承
     * （疑惑）：为什么接口声明为private，给谁用？
     */
    class JsonDouble final : public Value<Json::NUMBER, double>{
        // 最原始的声明再JsonValue中
        double number_value() const override { return m_value; }
        int int_value() const override { return static_cast<int>(m_value); }
        bool equals(const JsonValue* other) const override{ return m_value == other->number_value(); }
        bool less(const JsonValue* other) const override  { return m_value <  other->number_value(); }

    public:
        explicit JsonDouble(double value) : Value(value) {}
    };

    class JsonInt final : public Value<Json::NUMBER, int>{
        double number_value() const override { return m_value; }
        int int_value() const override { return m_value; }
        bool equals(const JsonValue* other) const override{ return m_value == other->number_value(); }
        bool less(const JsonValue* other) const override{ return m_value < other->number_value(); }
    public:
        explicit JsonInt(int value) : Value(value) {}
    };

    class JsonBoolean final : public Value<Json::BOOL, bool>{
        bool bool_value() const override { return m_value; }
    public:
        explicit JsonBoolean(bool value) : Value(value) {}
    };

    class JsonString final : public Value<Json::STRING, string>{
        const string& string_value() const override{ return m_value; }
    public:
        explicit JsonString(const string& value) : Value(value) {}
        explicit JsonString(string&& value)      : Value(move(value)) {}
    };

    class JsonArray final : public Value<Json::ARRAY, Json::array>{
        const Json::array& array_items() const override { return m_value; }
        const Json& operator[](size_t i) const override;
    public:
        explicit JsonArray(const Json::array& value) : Value(value) {}
        explicit JsonArray(Json::array&& value)      : Value(move(value)) {}
    };

    class JsonObject final : public Value<Json::OBJECT, Json::object>{
        const Json::object& object_items() const override{ return m_value; }
        const Json& operator[](const string& key) const override;
    public:
        explicit JsonObject(const Json::object& value) : Value(value) {}
        explicit JsonObject(const Json::object&& value) : Value(move(value)) {}
    };

//    class JsonNull final : public Value<Json::NUL, NullStruct>{
//    public:
//        JsonNull() : Value({}) {}
//    };
    class JsonNull final : public Value<Json::NUL, NullStruct> {
    public:
        JsonNull() : Value({}) {}
    };

    /*
     * 静态全局变脸
     * 静态初始化保证安全？
     * 为了保证数据的一致性
     */
    struct Statics{
        const std::shared_ptr<JsonValue> null = make_shared<JsonNull>();
        const std::shared_ptr<JsonValue> t = make_shared<JsonBoolean>(true);
        const std::shared_ptr<JsonValue> f = make_shared<JsonBoolean>(false);
        const string empty_string;
        const vector<Json> empty_vector;
        const map<string, Json> empty_map;
        Statics() {}
    };

    // 创建这么一个静态常量
    static const Statics& statics(){
        static const Statics s{};
        return s;
    }
    static const Json& static_null(){
        // 这必须是分开的，因为Json()访问statics().null
        // 这个函数用于创建一个空的Json值
        static const Json json_null;
        return json_null;
    }


    /*
     * 访问器
     */
    Json::Type Json::type() const { return m_ptr->type(); }
    double Json::number_value() const { return m_ptr->number_value(); }
    int Json::int_value() const { return m_ptr->int_value(); }
    bool Json::bool_value() const { return m_ptr->bool_value(); }
    const string& Json::string_value() const { return m_ptr->string_value(); }
    const vector<Json>& Json::array_items() const { return m_ptr->array_items(); }
    const map<string, Json>& Json::object_items() const { return m_ptr->object_items(); }
    const Json& Json::operator[](size_t i) const { return (*m_ptr)[i]; }
    const Json& Json::operator[](const std::string &key) const { return (*m_ptr)[key]; }

    double JsonValue::number_value() const { return 0; }
    int JsonValue::int_value() const { return 0; }
    bool JsonValue::bool_value() const { return false; }
    const string& JsonValue::string_value() const { return statics().empty_string; }
    const vector<Json>& JsonValue::array_items() const { return statics().empty_vector; }
    const map<string, Json>& JsonValue::object_items() const { return statics().empty_map; }
    const Json& JsonValue::operator[](const std::string &key) const { return static_null(); }
    const Json& JsonValue::operator[](size_t i) const { return static_null(); }

    const Json& JsonObject::operator[](const std::string &key) const {
        auto iter = m_value.find(key);
        return (iter == m_value.end() ) ? static_null() : iter->second;
    }

    const Json& JsonArray::operator[](size_t i) const {
        if(i >= m_value.size()){
            return static_null();
        }
        else{
            return m_value[i];
        }
    }

    /*
     * 比较器
     */
    bool Json::operator==(const Json& other) const {
        if(m_ptr == other.m_ptr){
            return false;
        }
        if(m_ptr->type() != other.m_ptr->type()){
            return m_ptr->type() < other.m_ptr->type();
        }
        return m_ptr->less(other.m_ptr.get());
    }

    /*
     * 解析
     * C风格的字符串更适合打印错误消息
     */
    static inline string esc(char c){
        char buf[12];
        if(static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c)){
            snprintf(buf, sizeof(buf), "'%c", c);
        }
        else{
            snprintf(buf, sizeof(buf), "(%d)", c);
        }
        return string(buf);
    }

    static inline bool in_range(long x, long lower, long upper){
        return (x >= lower && x <= upper);
    }

    namespace{
        /*
         * Json解析器
         */
        struct JsonParser final{
            /*
             * 状态信息
             */
            const string& str;
            size_t i;
            string& err;
            bool failed;
            const JsonParse strategy;

            /*
             * 解析失败时的标记函数
             * fail(msg, err_ret = Json())
             */
            Json fail(string&& msg){
                return fail(move(msg), Json());
            }

            template<class T>
            T fail(string&& msg, const T err_ret){
                if(!failed){
                    err = std::move(msg);
                }
                failed = true;
                return err_ret;
            }

            /*
             * 将解析器向前移动
             * 直到不是空白字符
             * consume_whitespace
             */
            void consume_whitespace(){
                while(str[i] == ' ' || str[i] == '\r' || str[i] == '\n'){
                    i++;
                }
            }

            /*
             * 对注释进行处理
             * Json中的注释和C/C++中的一样
             */
            bool consume_comment(){
                // 是否找到目标位置
                bool comment_found = false;
                if(str[i] == '/'){
                    i++;
                    if(i == str.size()){
                        return fail("unexpected end of input after start of comment", false);
                    }
                    // 单行注释
                    if(str[i] == '/'){
                        i++;
                        while(i < str.size() && str[i] != '\n'){
                            i++;
                        }
                        comment_found = true;
                    }
                    // 多行注释
                    else if(str[i] == '*'){
                        i++;
                        if(i > str.size() - 2){
                            return fail("unexpected end of input multi-line comment", false);
                        }
                        while(!(str[i] == '*' && str[i+1] == '/')){
                            i++;
                            if(i > str.size() - 2){
                                return fail("unexpected end of input inside mutil-line comment", false);
                            }
                        }
                        i += 2;
                        comment_found = true;
                    }
                    else{
                        return fail("malformed comment", false);
                    }
                }
                return comment_found;
            }

            /*
             * 移动解析器
             * 直到其不是空白符也不是注释
             */
            void consume_garbage(){
                consume_whitespace();
                if(strategy == JsonParse::COMMENTS){
                    bool comment_found = false;
                    do{
                        comment_found = consume_comment();
                        if(failed){
                            return;
                        }
                        consume_whitespace();
                    }
                    while(comment_found);
                }
            }

            /*
             * 返回下一个非空白字符
             * 若是到达结尾都还还没找到，将会标记一个错误并返回0
             */
            char get_next_token(){
                consume_garbage();
                if(failed){
                    return static_cast<char>(0);
                }
                if(i == str.size()){
                    return fail("excepted end of input", static_cast<char>(0));
                }
                return str[i+1];
            }

            /*
             * 将unicode字符转换为utf-8，并添加到out中
             */
            void encode_utf8(long pt, string & out) {
                if (pt < 0)
                    return;

                if (pt < 0x80) {
                    out += static_cast<char>(pt);
                } else if (pt < 0x800) {
                    out += static_cast<char>((pt >> 6) | 0xC0);
                    out += static_cast<char>((pt & 0x3F) | 0x80);
                } else if (pt < 0x10000) {
                    out += static_cast<char>((pt >> 12) | 0xE0);
                    out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
                    out += static_cast<char>((pt & 0x3F) | 0x80);
                } else {
                    out += static_cast<char>((pt >> 18) | 0xF0);
                    out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
                    out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
                    out += static_cast<char>((pt & 0x3F) | 0x80);
                }
            }

            /*
             * 解析字符串
             * 从当前位置开始解析
             *
             * 这段代码对编码的认识程序要求很高
             * 而我只熟悉ASCII，对unicode不懂
             * 因此直接复制的大佬源码，请谅解
             */
            string parse_string() {
                string out;
                long last_escaped_codepoint = -1;
                while (true) {
                    if (i == str.size())
                        return fail("unexpected end of input in string", "");

                    char ch = str[i++];

                    if (ch == '"') {
                        encode_utf8(last_escaped_codepoint, out);
                        return out;
                    }

                    if (in_range(ch, 0, 0x1f))
                        return fail("unescaped " + esc(ch) + " in string", "");

                    // The usual case: non-escaped characters
                    if (ch != '\\') {
                        encode_utf8(last_escaped_codepoint, out);
                        last_escaped_codepoint = -1;
                        out += ch;
                        continue;
                    }

                    // Handle escapes
                    if (i == str.size())
                        return fail("unexpected end of input in string", "");

                    ch = str[i++];

                    if (ch == 'u') {
                        // Extract 4-byte escape sequence
                        string esc = str.substr(i, 4);
                        // Explicitly check length of the substring. The following loop
                        // relies on std::string returning the terminating NUL when
                        // accessing str[length]. Checking here reduces brittleness.
                        if (esc.length() < 4) {
                            return fail("bad \\u escape: " + esc, "");
                        }
                        for (size_t j = 0; j < 4; j++) {
                            if (!in_range(esc[j], 'a', 'f') && !in_range(esc[j], 'A', 'F')
                                && !in_range(esc[j], '0', '9'))
                                return fail("bad \\u escape: " + esc, "");
                        }

                        long codepoint = strtol(esc.data(), nullptr, 16);

                        // JSON specifies that characters outside the BMP shall be encoded as a pair
                        // of 4-hex-digit \u escapes encoding their surrogate pair components. Check
                        // whether we're in the middle of such a beast: the previous codepoint was an
                        // escaped lead (high) surrogate, and this is a trail (low) surrogate.
                        if (in_range(last_escaped_codepoint, 0xD800, 0xDBFF)
                            && in_range(codepoint, 0xDC00, 0xDFFF)) {
                            // Reassemble the two surrogate pairs into one astral-plane character, per
                            // the UTF-16 algorithm.
                            encode_utf8((((last_escaped_codepoint - 0xD800) << 10)
                                         | (codepoint - 0xDC00)) + 0x10000, out);
                            last_escaped_codepoint = -1;
                        } else {
                            encode_utf8(last_escaped_codepoint, out);
                            last_escaped_codepoint = codepoint;
                        }

                        i += 4;
                        continue;
                    }

                    encode_utf8(last_escaped_codepoint, out);
                    last_escaped_codepoint = -1;

                    if (ch == 'b') {
                        out += '\b';
                    } else if (ch == 'f') {
                        out += '\f';
                    } else if (ch == 'n') {
                        out += '\n';
                    } else if (ch == 'r') {
                        out += '\r';
                    } else if (ch == 't') {
                        out += '\t';
                    } else if (ch == '"' || ch == '\\' || ch == '/') {
                        out += ch;
                    } else {
                        return fail("invalid escape character " + esc(ch), "");
                    }
                }
            }

            /*
             * 对double类型进行解析
             */
            Json parse_number() {
                size_t start_pos = i;

                if (str[i] == '-')
                    i++;

                // Integer part
                if (str[i] == '0') {
                    i++;
                    if (in_range(str[i], '0', '9'))
                        return fail("leading 0s not permitted in numbers");
                } else if (in_range(str[i], '1', '9')) {
                    i++;
                    while (in_range(str[i], '0', '9'))
                        i++;
                } else {
                    return fail("invalid " + esc(str[i]) + " in number");
                }

                if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
                    && (i - start_pos) <= static_cast<size_t>(std::numeric_limits<int>::digits10)) {
                    return std::atoi(str.c_str() + start_pos);
                }

                // Decimal part
                if (str[i] == '.') {
                    i++;
                    if (!in_range(str[i], '0', '9'))
                        return fail("at least one digit required in fractional part");

                    while (in_range(str[i], '0', '9'))
                        i++;
                }

                // Exponent part
                if (str[i] == 'e' || str[i] == 'E') {
                    i++;

                    if (str[i] == '+' || str[i] == '-')
                        i++;

                    if (!in_range(str[i], '0', '9'))
                        return fail("at least one digit required in exponent");

                    while (in_range(str[i], '0', '9'))
                        i++;
                }

                return std::strtod(str.c_str() + start_pos, nullptr);
            }

            /* expect(str, res)
             *
             * Expect that 'str' starts at the character that was just read. If it does, advance
             * the input and return res. If not, flag an error.
             */
            Json expect(const string &expected, Json res) {
                assert(i != 0);
                i--;
                if (str.compare(i, expected.length(), expected) == 0) {
                    i += expected.length();
                    return res;
                } else {
                    return fail("parse error: expected " + expected + ", got " + str.substr(i, expected.length()));
                }
            }

            /* parse_json()
             *
             * Parse a JSON object.
             */
            Json parse_json(int depth) {
                if (depth > max_depth) {
                    return fail("exceeded maximum nesting depth");
                }

                char ch = get_next_token();
                if (failed)
                    return Json();

                if (ch == '-' || (ch >= '0' && ch <= '9')) {
                    i--;
                    return parse_number();
                }

                if (ch == 't')
                    return expect("true", true);

                if (ch == 'f')
                    return expect("false", false);

                if (ch == 'n')
                    return expect("null", Json());

                if (ch == '"')
                    return parse_string();

                if (ch == '{') {
                    map<string, Json> data;
                    ch = get_next_token();
                    if (ch == '}')
                        return data;

                    while (1) {
                        if (ch != '"')
                            return fail("expected '\"' in object, got " + esc(ch));

                        string key = parse_string();
                        if (failed)
                            return Json();

                        ch = get_next_token();
                        if (ch != ':')
                            return fail("expected ':' in object, got " + esc(ch));

                        data[std::move(key)] = parse_json(depth + 1);
                        if (failed)
                            return Json();

                        ch = get_next_token();
                        if (ch == '}')
                            break;
                        if (ch != ',')
                            return fail("expected ',' in object, got " + esc(ch));

                        ch = get_next_token();
                    }
                    return data;
                }

                if (ch == '[') {
                    vector<Json> data;
                    ch = get_next_token();
                    if (ch == ']')
                        return data;

                    while (1) {
                        i--;
                        data.push_back(parse_json(depth + 1));
                        if (failed)
                            return Json();

                        ch = get_next_token();
                        if (ch == ']')
                            break;
                        if (ch != ',')
                            return fail("expected ',' in list, got " + esc(ch));

                        ch = get_next_token();
                        (void)ch;
                    }
                    return data;
                }

                return fail("expected value, got " + esc(ch));
            }
            /******************* 复制部分结束 ********************/
        };
    } // namespace none


    Json Json::parse(const string& in, string& err, JsonParse strategy){
        JsonParser parser {in, 0, err, false, strategy};
        Json result = parser.parse_json(0);

        // 检查是否有不必要的“垃圾”跟随在尾部
        parser.consume_garbage();
        if(parser.failed){
            return Json();
        }
        if(parser.i != in.size()){
            return parser.fail("unexpected trailing" + esc(in[parser.i]));
        }
        return result;
    }

    /*
     * 记录在.h文件中
     */
    vector<Json> Json::parse_multi(const string& in,
                                   std::string::size_type& parser_stop_pos,
                                   string& err,
                                   JsonParse strategy){
        JsonParser parser {in, 0, err, false, strategy};
        parser_stop_pos = 0;
        vector<Json> json_vec;
        while(parser.i != in.size() && !parser.failed){
            json_vec.push_back(parser.parse_json(0));
            if(parser.failed){
                break;
            }

            // 检查其它对象
            parser.consume_garbage();
            if(parser.failed){
                break;
            }
            parser_stop_pos = parser.i;
        }
        return json_vec;
    }

    /*
     * 对Json数据的结构进行检查
     */
    bool Json::has_shape(const shape & types, string & err) const {
        if (!is_object()) {
            err = "expected JSON object, got " + dump();
            return false;
        }

        const auto& obj_items = object_items();
        for (auto & item : types) {
            const auto it = obj_items.find(item.first);
            if (it == obj_items.cend() || it->second.type() != item.second) {
                err = "bad type for " + item.first + " in " + dump();
                return false;
            }
        }
        return true;
    }
}