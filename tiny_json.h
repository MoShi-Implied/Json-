#pragma once

#include <string>
#include <vector>
#include <map>
// 智能指针所在的头文件
#include <memory>
// 初始化列表
#include <initializer_list>

/*
 * 用户检查VS的版本
 * 知道它为什么这么写
 * 但是要我肯定是写不出这种代码的
 */
#ifdef _MSC_VER
#if _MSC_VER <= 1800 // VS 2013
        #ifndef noexcept
            // 这种用法不是很懂，为什么将函数设置为一个宏，如何使用？
            #define noexcept throw()
        #endif

        #ifndef snprintf
            #define snprintf _snprintf_s
        #endif
    #endif
#endif

namespace json11{
    // Json解析状态
    enum JsonParse{
        STANDARD, COMMENTS
    };

    /*
     * 类的提前声明
     * 这个类目前还没有实现
     * 但是这句话是告诉编译器这个类是真实存在的
     */
    class JsonValue;

    /*
     * final表示该类是一个最终类
     * 它不能拥有子类
     * 不可继承
     */
    class Json final{
    public:
        // Json类型
        // 涵盖了Json中的所有数据类型
        enum Type{
            NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT
        };

        /*
         * 起别名
         * 使用C++中的数据类型来存储Json数据
         * array对应vector
         * object对应map
         */
        typedef std::vector<Json> array;
        // 不太明白为什么是string对应的Json
        typedef std::map<std::string, Json> object;

        // 构建不同Json值的方法
        Json() noexcept;
        // std::nullptr_t就是C++中nullptr的字面符常量
        Json(std::nullptr_t) noexcept;
        Json(double value);
        Json(int value);
        Json(bool value);
        Json(const std::string& value);
        Json(std::string&& value);
        Json(const char* value);
        Json(array&& values);
        Json(const object& values);
        Json(object&& values);

        /*
         * 原作者说是隐式构造函数？
         * 翻译：隐式构造函数，任何带有 to_json() 函数的东西
         * 在笔记中有写
         */
        template<class T, class = decltype(&T::to_json)>
        Json(const T& t) : Json(t.to_json()) {}

        /*
         * 模板的高级用法
         * 已经看不懂、汗流浃背了
         * 先放着，以后再来看
         */
        template <class M, typename std::enable_if<
                std::is_constructible<std::string, decltype(std::declval<M>().begin()->first)>::value
                && std::is_constructible<Json, decltype(std::declval<M>().begin()->second)>::value,
                int>::type = 0>
        Json(const M & m) : Json(object(m.begin(), m.end())) {}
        template <class V, typename std::enable_if<
                std::is_constructible<Json, decltype(*std::declval<V>().begin())>::value,
                int>::type = 0>
        Json(const V & v) : Json(array(v.begin(), v.end())) {}

        /*
         * 避免指针类型被隐式转换成bool类型
         */
        Json(void*) = delete;

        /*
         * 访问器函数，用于获取Json数据的类型
         */
        Type type() const;

        bool is_null()      const { return type() == NUL; }
        bool is_number()    const { return type() == NUMBER; }
        bool is_bool()      const { return type() == BOOL; }
        bool is_string()    const { return type() == STRING; }
        bool is_array()     const { return type() == ARRAY; }
        bool is_object()    const { return type() == OBJECT; }

        /*
         * 在该json项目中
         * 不会区分整数和非整数
         * number_value()和int_value()都可用于NUMBER类型
         * 疑惑：为什么不能就使用一个number_value完成所有操作？
         */
        double number_value() const;
        int int_value() const;

        bool bool_value() const;

        const std::string& string_value() const;

        const array& array_items() const;
        const object& object_items() const;

        // 如果是一个array，返回arr[i]
        const Json& operator[](size_t i) const;
        // 如果是一个object，返回obj[key]
        const Json& operator[](const std::string& key) const;

        /*
         * 序列化
         * 也就是将数据结构转换为Json数据
         */
        void dump(std::string& out) const;
        std::string dump() const {
            std::string out;
            dump(out);
            return out;
        }

        /*
         * 解析
         * 如果解析失败，则返回 Json()并将错误消息分配给err
         */
        static Json parse(const std::string& in,
                          std::string& err,
                          JsonParse strategy = JsonParse::STANDARD);
        static Json parse(const char* in,
                          std::string& err,
                          JsonParse strategy = JsonParse::STANDARD){
            if(in){
                return parse(std::string(in), err, strategy);
            }
            else{
                err = "null input";
                return nullptr;
            }
        }

        /*
         * 解析多个对象
         * 串联或用空格分隔
         * 不是很明白这两个函数的作用
         */
        static std::vector<Json> parse_multi(
                const std::string& in,
                std::string::size_type& parser_stop_pos,
                std::string& err,
                JsonParse strategy = JsonParse::STANDARD
                );
        static inline std::vector<Json> parse_multi(
                const std::string& in,
                std::string& err,
                JsonParse strategy = JsonParse::STANDARD){
            std::string::size_type parser_stop_pos;
            return parse_multi(in, parser_stop_pos, err, strategy);
        }

        bool operator==(const Json& rhs) const;
        bool operator< (const Json& rhs) const;
        bool operator!=(const Json& rhs) const { return !(*this == rhs); }
        bool operator<=(const Json& rhs) const { return !(rhs < *this); }
        bool operator> (const Json& rhs) const { return  (rhs < *this); }
        bool operator>=(const Json& rhs) const { return !(*this < rhs); }


        /*
         * has_shape()用于检查Json对象是否正确
         * 若传入的是一个Json object，看每个指定类型的数据是否都包含在其中
         * 若是传入的对象不是Json object，或者object中缺少指定类型的字段，就会返回false，并设置err
         * 否则返回true
         */
        // 显式初始化列表
        typedef std::initializer_list<std::pair<std::string, Type>> shape;
        bool has_shape(const shape& types, std::string& err) const;

    private:
        // 这个指针指向的是什么？
        // 似乎是指需要解析的Json数据
        std::shared_ptr<JsonValue> m_ptr;
     };



    /*
     * 抽象类JsonValue
     */
    class JsonValue{
    protected:
        // 为什么使用的是友元？
        friend class Json;
        friend class JsonInt;
        friend class JsonDouble;

        virtual Json::Type type() const = 0;
        virtual bool equals(const JsonValue* other) const = 0;
        virtual bool less(const JsonValue* other) const = 0;
        virtual void dump(std::string& out) const = 0;
        virtual double number_value() const;
        virtual int int_value() const;
        virtual bool bool_value() const;
        virtual const std::string& string_value() const;
        virtual const Json::array& array_items() const;
        virtual const Json& operator[](size_t i) const;
        virtual const Json::object& object_items() const;
        virtual const Json& operator[](const std::string& key) const;
        virtual ~JsonValue() {}
    };
} // namespace json11