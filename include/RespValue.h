#ifndef RESPVALUE_H
#define RESPVALUE_H

#include <string>

enum class RespType {
    SimpleString,
    BulkString,
    Integer,
    Array,
    SimpleError,
    Null
};

class RespValue {
protected:
    explicit RespValue() = default;
public:
    virtual ~RespValue() = default;
    virtual RespType getType() const = 0;
};

class RespSString : public RespValue {
public:
    RespSString() = default;

    RespType getType() const override {
        return RespType::SimpleString;
    }
};

class RespBString : public RespValue {
public:
    RespBString() = default;

    RespType getType() const override {
        return RespType::BulkString;
    }
};

class RespInteger : public RespValue {
public:
    RespInteger() = default;

    RespType getType() const override {
        return RespType::Integer;
    }
};

class RespArray : public RespValue {
public:
    RespArray() = default;

    RespType getType() const override {
        return RespType::Array;
    }
};

class RespSError : public RespValue {
public:
    RespSError() = default;

    RespType getType() const override {
        return RespType::SimpleError;
    }
};

class RespNull : public RespValue {
public:
    RespNull() = default;

    RespType getType() const override {
        return RespType::Null;
    }
};

#endif
