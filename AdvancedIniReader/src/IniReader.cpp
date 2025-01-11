#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>

#define print(x) std::cout << x << std::endl;

namespace Config{
    void Strip(std::string& str){
        str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), [](char ch){ return std::isspace(ch); } ));
        str.erase(std::find_if_not(str.rbegin(), str.rend(), [](char ch){ return std::isspace(ch); }).base(), str.end());
    }

    bool IsAlpha(const std::string& str){
        return std::all_of(str.begin(), str.end(), [](char ch) { return std::isalpha(ch); });
    }

    bool IsSpace(const std::string& str){
        return std::all_of(str.begin(), str.end(), [](char ch) { return std::isspace(ch); });
    }

    class Item{
        private:
        std::string m_key;
        std::string m_value;
        public:
        Item(const std::string& key, const std::string& value) : m_key(key), m_value(value) {}

        template<typename Type>
        Type Get() const {
            print(m_value);
            std::istringstream iss(m_value);
            Type value;
            if (!(iss >> value))
                throw std::invalid_argument("Невозможно привести к типу");
            
            return value;
        }

        template<>
        std::string Get() const{
            return m_value;
        }


        template<typename Type>
        void Set(const Type& value){
            std::ostringstream oss;
            oss << value;
            m_value = oss.str();
        }


        friend std::ostream& operator<<(std::ostream& os, const Item& item) {
            os << item.m_key << "=" << item.m_value;
            return os;
        }
    };

    class Section{
        private:
        std::string m_sectionName;
        std::vector<std::string> m_itemOrder;
        std::unordered_map<std::string, Item> m_item;

        public:
        Section(const std::string& name) : m_sectionName(name) {}

        void Add(const std::string& key, const std::string& value) {
            m_itemOrder.emplace_back(key);
            m_item.emplace(key, Item(key, value));
        }

        Item& operator[](const char* key){
            return m_item.at(key);
        }

        const Item& operator[](const char* key) const{
            return m_item.at(key);
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
                if (line.empty() || line.front() == '#' || line.front() == ';')
                    continue;

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
                // 1. Обычная секция
                if (line.back() == ']')
                    sectionName = line.substr(1, line.size() - 2);

                // 2. Секция с комментарием
                else{
                    size_t closingBracketIdx = line.find(']');

                    if (closingBracketIdx == std::string::npos)
                        throw std::invalid_argument("Незаконченная секция");
                    
                    sectionName = line.substr(1, closingBracketIdx - 1);
                    VerifyComent(line.substr(closingBracketIdx+1));
                }

                if (!IsAlpha(sectionName))
                    throw std::invalid_argument("Секция должна содержать только буквенные знаки");

                m_sectionOrder.emplace_back(sectionName);
                m_section.emplace(sectionName, Section(sectionName));
                activeSection = &m_section.at(sectionName);
                return true;
            }
            return false;
        }

        bool ParseItem(){
            size_t equalSing =  std::count(line.begin(), line.end(), '=');
                
            if (equalSing != 1)
                throw std::invalid_argument("Syntax error. Пара должна содержать один знак =");

            if (!activeSection)
                throw std::invalid_argument("Парсер неподдерживает глобальные переменные в ini-файле");

            size_t delimIdx = line.find("=");

            // 1. Key
            std::string key = line.substr(0, delimIdx);
            Strip(key);

            if (!IsAlpha(key))
                throw std::invalid_argument("Ключ должен содержать только буквенные символы");

            // 2. Value
            std::string value = line.substr(delimIdx+1);
            Strip(value);

            // 2.1 String
            if (value.front() == '\"' || value.front() == '\'')
                ParseString(value);
            // 2.2 Int & Float
            else
                ParseNumber(value);

            activeSection->Add(key,value);
            return true;
        }

        void ParseString(std::string& value){
            char leftQMark;
            char rightQMark;
            leftQMark = value.front();

            size_t rightQMarkIdx = value.find(leftQMark, 1);

            if (rightQMarkIdx == std::string::npos)
                throw std::invalid_argument("Строка записана неправильно");
            else if (rightQMarkIdx == value.size() -1){
                value = value.substr(1, value.size() - 2);
                return;
            }

            VerifyComent(value.substr(rightQMarkIdx+1));
            value = value.substr(1, rightQMarkIdx-1);
        }

        void ParseNumber(std::string& value){
            size_t comentBeginIdx = value.find_first_of("#; ");
            if (comentBeginIdx != std::string::npos){
                value = value.substr(0, comentBeginIdx);
                VerifyComent(value.substr(comentBeginIdx));
            }

            size_t dotCount = 0;
            for(size_t i=0; i<value.size(); ++i){
                if (i==0 && i+1 == value.size() && value[i] == '.')
                    throw std::invalid_argument("Неправильная запись числа");

                if (value[i] =='.' && ++dotCount > 1)
                    throw std::invalid_argument("Число не может содержать больше одной точки");

                if (!(std::isdigit(value[i]) || value[i] == '.'))
                    throw std::invalid_argument("Число должно содержать только цифры");
            }
        }

        void VerifyComent(const std::string& str){
            size_t comentSignIdx = str.find_first_of("#;");
            std::string space = str.substr(0, comentSignIdx);

            if (!IsSpace(space))
                throw std::invalid_argument("Неожиданные символы. Если вы хотели оставить комментарий используйте # или ;");
        }


        public:
        INI(const std::string& filename) : m_filename(filename) {
            std::ifstream file(m_filename);
            if (!file.is_open())
                throw std::invalid_argument("Парсер не смог открыть файл");

            Parse(file);
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
    };
}


using Ini = Config::INI;
using Section = Config::Section;
using Item = Config::Item;

int main(){

}