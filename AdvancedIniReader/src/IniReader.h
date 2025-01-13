#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "IniReaderExceptions.h"

namespace Utils {
    void Strip(std::string& str) {
        str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), [](char ch) { return std::isspace(ch); }));
        str.erase(std::find_if_not(str.rbegin(), str.rend(), [](char ch) { return std::isspace(ch); }).base(), str.end());
    }

    bool IsAlpha(const std::string& str) {
        return std::all_of(str.begin(), str.end(), [](char ch) { return std::isalnum(ch); });
    }

    bool IsSpace(const std::string& str) {
        return std::all_of(str.begin(), str.end(), [](char ch) { return std::isspace(ch); });
    }
}

namespace Config{
    struct ParsedValue {
        std::string value;
        std::string coment;
    };

    class Item{
        private:
        std::string m_key;
        std::string m_value;
        std::string m_coment = "";
        bool m_hasComent = false;

        public:
        Item(const std::string& key, const ParsedValue& parsedValue) : m_key(key), m_value(parsedValue.value) {
            if (!parsedValue.coment.empty()) {
                m_coment = parsedValue.coment;
                m_hasComent = true;
            }
        }

        Item() = default;

        template<typename Type>
        Type Get() const {
            std::istringstream iss(m_value);
            Type value;
            if (!(iss >> value))
                 throw InvalidCastException(std::string("It's impossible to case \"") + m_value + "\" to the passed type");

            return value;
        }

        template<>
        std::string Get() const{
            if (m_value.front() == '\'' || m_value.front() == '\"')
                return m_value.substr(1, m_value.size() -2);
            return m_value;
        }

        std::string GetKey() const {
            return m_key;
        }

        template<typename Type>
        void Set(const Type& value){
            std::ostringstream oss;
            oss << value;
            m_value = oss.str();
        }

        template<>
        void Set(const std::string& value) {
            m_value = '\"' + value + '\"';
        }

        friend std::ostream& operator<<(std::ostream& os, const Item& item) {
            std::string coment = item.m_hasComent ? " " + item.m_coment : "";
            os << item.m_key << "=" << item.m_value << coment;
            return os;
        }
    };

    class Section {
    private:
        friend class INI;

        std::string m_sectionName;
        std::vector<std::string> m_itemOrder;
        std::unordered_map<std::string, Item> m_item;
    private:
        void Add(const std::string& key, const ParsedValue& parsedValue) {
            m_itemOrder.emplace_back(key);
            m_item.emplace(key, Item(key, parsedValue));
        }

        void CheckKey(const std::string& key) {
            if (!Utils::IsAlpha(key))
                throw KeyException("The key must contain only letters");

            if (m_item.find(key) != m_item.end())
                throw ItemException("Item with key \"" + key + "\" already exists");
        }

    public:
        Section(const std::string& name) : m_sectionName(name) {}

        template<typename Type>
        void AddItem(const std::string& key, const Type& value) {
            CheckKey(key);
            std::ostringstream oss;
            oss << value;
            Add(key, { oss.str(), "" });
        }

        template<>
        void AddItem(const std::string& key, const std::string& value) {
            CheckKey(key);
            Add(key, { '\"' + value + '\"', ""});
        }

        Item& operator[](const char* key) {
            try {
                return m_item.at(key);
            }
            catch (const std::out_of_range&) {
                throw ItemException(std::string("The item with the key \"") + key + "\" does not exist");
            }
        }

        bool HasItem(const std::string& key) {
            return m_item.find(key) != m_item.end();
        }

        class Iterator {
        private:
            std::string* m_ptr;
            Section* m_section;

        public:
            Iterator(std::string* ptr, Section* section) : m_ptr(ptr), m_section(section) {}

            Item& operator*() {
                return m_section->m_item.at(*m_ptr);
            }

            bool operator!=(const Iterator& other) const {
                return m_ptr != other.m_ptr;
            }

            Iterator& operator++() {
                ++m_ptr;
                return *this;
            }

            Iterator operator++(int) {
                return Iterator(m_ptr++, m_section);
            }
        };

        class ConstIterator {
        private:
            const std::string* m_ptr;
            const Section* m_section;

        public:
            ConstIterator(const std::string* ptr, const Section* section) : m_ptr(ptr), m_section(section) {}

            const Item& operator*() const {
                return m_section->m_item.at(*m_ptr);
            }

            bool operator!=(const ConstIterator& other) const {
                return m_ptr != other.m_ptr;
            }

            ConstIterator& operator++() {
                ++m_ptr;
                return *this;
            }

            ConstIterator operator++(int){
                return ConstIterator(m_ptr++, m_section);
            }
        };

        Iterator begin() { return Iterator(&(*m_itemOrder.begin()), this); }

        Iterator end() { return Iterator(m_itemOrder.data() + m_itemOrder.size(), this); }
        
        ConstIterator begin() const { return cbegin(); }

        ConstIterator end() const { return cend(); }

        ConstIterator cbegin() const { return ConstIterator(&(*m_itemOrder.begin()), this); }

        ConstIterator cend() const { return ConstIterator(m_itemOrder.data() + m_itemOrder.size(), this); }

        friend std::ostream& operator<<(std::ostream& os, const Section& section) {
            os << '[' << section.m_sectionName << ']' << std::endl;
            bool firstLine = true;

            for (const auto& item : section) {
                os << (firstLine ? "" : "\n") << item;
                firstLine = false;
            }
            return os;
        }
    };

    class INI{
        private:
        std::string m_filename;
        bool m_writeState = false;

        std::vector<std::string> m_sectionOrder;
        std::unordered_map<std::string, Section> m_section;

        std::string line;
        Section* activeSection = nullptr;

        private:
        void Parse(std::ifstream& file){
            size_t lineIdx = 0;
            Section* activeSection = nullptr;

            while (std::getline(file, line)){
                ++lineIdx;

                // I. Empty or Commented line
                if (line.empty())
                    continue;

                if (line.front() == '#' || line.front() == ';')
                    throw std::invalid_argument("Сomments are not supported on individual lines");

                // II. Section
                if (!ParseSection()){
                    // III. Item
                    ParseItem();
                }
            }
        }

        bool ParseSection(){
            if (line.front() == '['){
                std::string sectionName;
                // 1. Section
                if (line.back() == ']')
                    sectionName = line.substr(1, line.size() - 2);

                // 2. Section with coment
                else{
                    size_t closingBracketIdx = line.find(']');

                    if (closingBracketIdx == std::string::npos)
                        throw SectionException("Unterminated section");
                    
                    sectionName = line.substr(1, closingBracketIdx - 1);
                    VerifyComent(line.substr(closingBracketIdx+1));
                }

                if (!Utils::IsAlpha(sectionName))
                    throw SectionException("Section must contain only letters");

                m_sectionOrder.emplace_back(sectionName);
                m_section.emplace(sectionName, Section(sectionName));
                activeSection = &m_section.at(sectionName);
                return true;
            }
            return false;
        }

        bool ParseItem() {
            size_t equalSing = std::count(line.begin(), line.end(), '=');

            if (equalSing != 1)
                throw ItemException("Syntax error. Item should contain only one '='");

            if (!activeSection)
                throw IniReaderException("Global variables are not supported");

            size_t delimIdx = line.find("=");

            // 1. Key
            std::string key = line.substr(0, delimIdx);
            Utils::Strip(key);

            if (!Utils::IsAlpha(key))
                throw KeyException("The key must contain only letters");

            // 2. Value
            ParsedValue parsedValue; 

            std::string value = line.substr(delimIdx + 1);
            Utils::Strip(value);

            // 2.1 String
            if (value.front() == '\"' || value.front() == '\'')
                parsedValue = ParseString(value);
            // 2.2 Int & Float
            else
                parsedValue = ParseNumber(value);

            activeSection->Add(key, parsedValue);

            return true;
        }

        ParsedValue ParseString(std::string& value){
            char leftQMark;
            leftQMark = value.front();

            size_t rightQMarkIdx = value.find(leftQMark, 1);

            if (rightQMarkIdx == std::string::npos)
                throw ValueException("Mismatched or unterminated bracket");
            else if (rightQMarkIdx == value.size() -1){
                return { value , ""};
            }

            return { value.substr(0, rightQMarkIdx+1), VerifyComent(value.substr(rightQMarkIdx + 1)) };
        }

        ParsedValue ParseNumber(std::string& value){
            size_t comentBeginIdx = value.find_first_of("#; ");
            ParsedValue result;

            if (comentBeginIdx != std::string::npos){ 
                result = { value.substr(0, comentBeginIdx), VerifyComent(value.substr(comentBeginIdx)) };
            }
            else {
                result = { value, "" };
            }

            size_t dotCount = 0;
            for(size_t i=0; i<result.value.size(); ++i){
                if (i==0 && i+1 == result.value.size() && result.value[i] == '.')
                    throw ValueException("Wrong number");

                if (result.value[i] =='.' && ++dotCount > 1)
                    throw ValueException("Wrong number");

                if (!(std::isdigit(result.value[i]) || result.value[i] == '.'))
                    throw ValueException("Wrong number");
            }
            return result;
        }

        // Verify & Return Coment
        std::string VerifyComent(const std::string& str) {
            size_t comentSignIdx = str.find_first_of("#;");
            std::string space = str.substr(0, comentSignIdx);

            if (!Utils::IsSpace(space))
                throw IniReaderException("Syntax Error. Unexpected symbols");

            return str.substr(comentSignIdx);
        }

        public:
        INI(const std::string& filename) : m_filename(filename) {
            std::ifstream file(m_filename);
            if (!file.is_open())
                throw IniReaderException("Can't open file for reading");

            Parse(file);
            file.close();
        }

        Section& operator[](const char* key){
            return m_section.at(key);
        }

        const Section& operator[](const char* key) const{
            return m_section.at(key);
        }

        friend std::ostream& operator<<(std::ostream& os, const INI& ini) {
            bool firstLine = true;
            for (const auto& sectionName : ini.m_sectionOrder) {
                os << (firstLine ? "" : "\n\n") << ini.m_section.at(sectionName);
                firstLine = false;
            }
            return os;
        }

        void Save() {
            m_writeState = true;

            std::ofstream file(m_filename);
            if (!file.is_open())
                throw IniReaderException("Can't open file for writing");

            file << *this;
            file.close();
            m_writeState = false;
        }
    };
}