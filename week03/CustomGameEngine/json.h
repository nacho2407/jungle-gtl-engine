#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <type_traits>

namespace json {

    using std::map;
    using std::deque;
    using std::string;
    using std::enable_if;
    using std::initializer_list;
    using std::is_same;
    using std::is_convertible;
    using std::is_integral;
    using std::is_floating_point;

    class JSON
    {
        union BackingData {
            BackingData(double d) : Float(d) {}
            BackingData(long   l) : Int(l) {}
            BackingData(bool   b) : Bool(b) {}
            BackingData(string s) : String(new string(s)) {}
            BackingData() : Int(0) {}

            deque<JSON>* List;
            map<string, JSON>* Map;
            string* String;
            double              Float;
            long                Int;
            bool                Bool;
        } Internal;

    public:
        enum class Class {
            Null,
            Object,
            Array,
            String,
            Floating,
            Integral,
            Boolean
        };

        template <typename Container>
        class JSONWrapper {
            Container* object;
        public:
            JSONWrapper(Container* val) : object(val) {}
            JSONWrapper(std::nullptr_t) : object(nullptr) {}
            typename Container::iterator begin() { return object ? object->begin() : typename Container::iterator(); }
            typename Container::iterator end() { return object ? object->end() : typename Container::iterator(); }
            typename Container::const_iterator begin() const { return object ? object->begin() : typename Container::iterator(); }
            typename Container::const_iterator end() const { return object ? object->end() : typename Container::iterator(); }
        };

        template <typename Container>
        class JSONConstWrapper {
            const Container* object;
        public:
            JSONConstWrapper(const Container* val) : object(val) {}
            JSONConstWrapper(std::nullptr_t) : object(nullptr) {}
            typename Container::const_iterator begin() const { return object ? object->begin() : typename Container::const_iterator(); }
            typename Container::const_iterator end() const { return object ? object->end() : typename Container::const_iterator(); }
        };

        JSON();
        JSON(initializer_list<JSON> list);
        JSON(JSON&& other);
        JSON& operator=(JSON&& other);
        JSON(const JSON& other);
        JSON& operator=(const JSON& other);
        ~JSON();

        // Template Constructors
        template <typename T>
        JSON(T b, typename enable_if<is_same<T, bool>::value>::type* = 0) : Internal(b), Type(Class::Boolean) {}

        template <typename T>
        JSON(T i, typename enable_if<is_integral<T>::value && !is_same<T, bool>::value>::type* = 0) : Internal((long)i), Type(Class::Integral) {}

        template <typename T>
        JSON(T f, typename enable_if<is_floating_point<T>::value>::type* = 0) : Internal((double)f), Type(Class::Floating) {}

        template <typename T>
        JSON(T s, typename enable_if<is_convertible<T, string>::value>::type* = 0) : Internal(string(s)), Type(Class::String) {}

        JSON(std::nullptr_t);

        static JSON Make(Class type);
        static JSON Load(const string&);

        // Template Methods
        template <typename T>
        void append(T arg) {
            SetType(Class::Array); Internal.List->emplace_back(arg);
        }

        template <typename T, typename... U>
        void append(T arg, U... args) {
            append(arg); append(args...);
        }

        template <typename T>
        typename enable_if<is_same<T, bool>::value, JSON&>::type operator=(T b) {
            SetType(Class::Boolean); Internal.Bool = b; return *this;
        }

        template <typename T>
        typename enable_if<is_integral<T>::value && !is_same<T, bool>::value, JSON&>::type operator=(T i) {
            SetType(Class::Integral); Internal.Int = i; return *this;
        }

        template <typename T>
        typename enable_if<is_floating_point<T>::value, JSON&>::type operator=(T f) {
            SetType(Class::Floating); Internal.Float = f; return *this;
        }

        template <typename T>
        typename enable_if<is_convertible<T, string>::value, JSON&>::type operator=(T s) {
            SetType(Class::String);
            if (Internal.String) *Internal.String = string(s);
            else Internal.String = new string(s);
            return *this;
        }

        JSON& operator[](const string& key);
        JSON& operator[](unsigned index);
        JSON& at(const string& key);
        const JSON& at(const string& key) const;
        JSON& at(unsigned index);
        const JSON& at(unsigned index) const;

        int length() const;
        bool hasKey(const string& key) const;
        int size() const;
        Class JSONType() const { return Type; }

        bool IsNull() const { return Type == Class::Null; }

        string ToString() const;
        string ToString(bool& ok) const;
        double ToFloat() const;
        double ToFloat(bool& ok) const;
        long ToInt() const;
        long ToInt(bool& ok) const;
        bool ToBool() const;
        bool ToBool(bool& ok) const;

        JSONWrapper<map<string, JSON>> ObjectRange();
        JSONWrapper<deque<JSON>> ArrayRange();
        JSONConstWrapper<map<string, JSON>> ObjectRange() const;
        JSONConstWrapper<deque<JSON>> ArrayRange() const;

        string dump(int depth = 1, string tab = "  ") const;

        friend std::ostream& operator<<(std::ostream&, const JSON&);

    private:
        void SetType(Class type);
        void ClearInternal();

    private:
        Class Type = Class::Null;
    };

    JSON Array();
    template <typename... T>
    JSON Array(T... args) {
        JSON arr = JSON::Make(JSON::Class::Array);
        arr.append(args...);
        return std::move(arr);
    }

    JSON Object();
    std::ostream& operator<<(std::ostream& os, const JSON& json);

} // End Namespace json