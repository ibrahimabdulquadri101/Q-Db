#include "Tokenizer.h"

#include <cctype>
#include <stdexcept>
#include <unordered_set>

std::vector<Token> tokenize(std::string input)
{
    static const std::unordered_set<std::string> keywords = {
        "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES"};

    std::vector<Token> tokens;
    std::size_t position = 0;

    while (position < input.size())
    {
        const unsigned char current = static_cast<unsigned char>(input[position]);
        if (std::isspace(current))
        {
            ++position;
            continue;
        }

        if (std::isalpha(current) || input[position] == '_')
        {
            const std::size_t start = position++;
            while (position < input.size())
            {
                const unsigned char character = static_cast<unsigned char>(input[position]);
                if (!std::isalnum(character) && input[position] != '_')
                {
                    break;
                }
                ++position;
            }

            std::string value = input.substr(start, position - start);
            std::string uppercase = value;
            for (char &character : uppercase)
            {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
            tokens.push_back({keywords.count(uppercase) ? TokenType::KEYWORD : TokenType::IDENTIFIER, value});
            continue;
        }

        if (std::isdigit(current))
        {
            const std::size_t start = position++;
            while (position < input.size() &&
                   std::isdigit(static_cast<unsigned char>(input[position])))
            {
                ++position;
            }
            tokens.push_back({TokenType::INTEGER, input.substr(start, position - start)});
            continue;
        }

        if (input[position] == '\'')
        {
            ++position;
            std::string value;
            bool closed = false;
            while (position < input.size())
            {
                if (input[position] == '\'')
                {
                    if (position + 1 < input.size() && input[position + 1] == '\'')
                    {
                        value += '\'';
                        position += 2;
                        continue;
                    }
                    ++position;
                    closed = true;
                    break;
                }
                value += input[position++];
            }
            if (!closed)
            {
                throw std::invalid_argument("Unterminated string literal");
            }
            tokens.push_back({TokenType::STRING, value});
            continue;
        }

        TokenType type;
        switch (input[position])
        {
        case '=':
        case '<':
        case '>':
            type = TokenType::OPERATOR;
            break;
        case ',':
            type = TokenType::COMMA;
            break;
        case '(':
            type = TokenType::LPAREN;
            break;
        case ')':
            type = TokenType::RPAREN;
            break;
        case '*':
            type = TokenType::STAR;
            break;
        default:
            throw std::invalid_argument("Unexpected character in input");
        }
        tokens.push_back({type, std::string(1, input[position])});
        ++position;
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}