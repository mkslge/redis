#ifndef VALUE_H
#define VALUE_H

#include <string>
#include <utility>
#include <variant>

class Value {
public:
    using Storage = std::variant<int, double, char, std::string>;

    Value(int value) : value_(value) {}
    Value(double value) : value_(value) {}
    Value(char value) : value_(value) {}
    Value(std::string value) : value_(std::move(value)) {}
    Value(const char* value) : value_(std::string(value)) {}

    template <typename T>
    bool is() const {
        return std::holds_alternative<T>(value_);
    }

    template <typename T>
    const T& get() const {
        return std::get<T>(value_);
    }

    template <typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), value_);
    }

    const Storage& storage() const {
        return value_;
    }

    bool operator==(const Value& other) const {
        return value_ == other.value_;
    }

private:
    Storage value_;
};

#endif //VALUE_H
