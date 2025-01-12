#pragma once
#include <stdexcept>

class IniReaderException : public std::exception {
private:
	std::string m_msg = "Unknown Error";
public:
	IniReaderException(const std::string& msg) : m_msg(msg) {}
	IniReaderException() = default;

	const char* what() const noexcept override { return m_msg.c_str(); }

	virtual ~IniReaderException() = default;
};

class InvalidCastException : public IniReaderException {
public:
	InvalidCastException(const std::string& msg) : IniReaderException("InvalidCastException: " + msg) {};
	InvalidCastException() : IniReaderException() {};
};


class ItemException : public IniReaderException {
public:
	ItemException(const std::string& msg) : IniReaderException("ItemException: " + msg) {};
	ItemException() : IniReaderException() {};
};


class KeyException : public ItemException {
public:
	KeyException(const std::string& msg) : ItemException("KeyException: " + msg) {};
	KeyException() : ItemException() {};
};

class ValueException : public ItemException {
public:
	ValueException(const std::string& msg) : ItemException("ValueException: " + msg) {};
	ValueException() : ItemException() {};
};



class SectionException : public IniReaderException {
public:
	SectionException(const std::string& msg) : IniReaderException("SectionException: " + msg) {};
	SectionException() : IniReaderException() {};
};