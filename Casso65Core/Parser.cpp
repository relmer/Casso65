#include "Pch.h"

#include "Parser.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SplitLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Parser::SplitLines (const std::string & source)
{
    std::vector<std::string> lines;
    std::string              line;
    std::istringstream       stream (source);

    while (std::getline (stream, line))
    {
        // Strip trailing \r if present (handles \r\n line endings)
        if (!line.empty () && line.back () == '\r')
        {
            line.pop_back ();
        }

        lines.push_back (line);
    }

    if (lines.empty ())
    {
        lines.push_back ("");
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripComments
//
////////////////////////////////////////////////////////////////////////////////

static std::string StripComments (const std::string & line)
{
    size_t pos = line.find (';');

    if (pos == std::string::npos)
    {
        return line;
    }

    return line.substr (0, pos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Trim
//
////////////////////////////////////////////////////////////////////////////////

static std::string Trim (const std::string & s)
{
    size_t start = s.find_first_not_of (" \t");

    if (start == std::string::npos)
    {
        return "";
    }

    size_t end = s.find_last_not_of (" \t");
    return s.substr (start, end - start + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpper
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpper (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseLine
//
////////////////////////////////////////////////////////////////////////////////

ParsedLine Parser::ParseLine (const std::string & line, int lineNumber)
{
    ParsedLine result  = {};
    result.lineNumber  = lineNumber;
    result.isEmpty     = true;
    result.isDirective = false;

    // Strip comments first
    std::string stripped = StripComments (line);
    std::string trimmed  = Trim (stripped);

    if (trimmed.empty ())
    {
        return result;
    }

    result.isEmpty = false;
    std::string remainder = trimmed;

    // Check for label (contains ':')
    size_t colonPos = remainder.find (':');

    if (colonPos != std::string::npos)
    {
        result.label = Trim (remainder.substr (0, colonPos));
        remainder    = Trim (remainder.substr (colonPos + 1));

        if (remainder.empty ())
        {
            return result;
        }
    }

    // Check for directive (starts with '.')
    if (!remainder.empty () && remainder[0] == '.')
    {
        result.isDirective = true;

        size_t spacePos = remainder.find_first_of (" \t");

        if (spacePos == std::string::npos)
        {
            result.directive = ToUpper (remainder);
        }
        else
        {
            result.directive    = ToUpper (remainder.substr (0, spacePos));
            result.directiveArg = Trim (remainder.substr (spacePos + 1));
        }

        return result;
    }

    // Extract mnemonic (first word)
    size_t spacePos = remainder.find_first_of (" \t");

    if (spacePos == std::string::npos)
    {
        result.mnemonic = ToUpper (remainder);
        return result;
    }

    result.mnemonic = ToUpper (remainder.substr (0, spacePos));
    result.operand  = Trim (remainder.substr (spacePos + 1));

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrimOperand
//
////////////////////////////////////////////////////////////////////////////////

static std::string TrimOperand (const std::string & s)
{
    size_t start = s.find_first_not_of (" \t");

    if (start == std::string::npos)
    {
        return "";
    }

    size_t end = s.find_last_not_of (" \t");
    return s.substr (start, end - start + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpperStr
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpperStr (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMatchingClose — find matching ')' or ']' respecting nesting
//
////////////////////////////////////////////////////////////////////////////////

static size_t FindMatchingClose (const std::string & s, size_t openPos)
{
    char openChar  = s[openPos];
    char closeChar = (openChar == '(') ? ')' : ']';
    int  depth     = 1;



    for (size_t i = openPos + 1; i < s.size (); i++)
    {
        char c = s[i];

        if (c == '\'' && i + 2 < s.size () && s[i + 2] == '\'')
        {
            i += 2;  // skip char literal
        }
        else if (c == openChar)
        {
            depth++;
        }
        else if (c == closeChar)
        {
            depth--;

            if (depth == 0)
            {
                return i;
            }
        }
    }

    return std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindTopLevelComma — find ',' not inside () [] or ''
//
////////////////////////////////////////////////////////////////////////////////

static size_t FindTopLevelComma (const std::string & s, size_t start = 0)
{
    int depth = 0;



    for (size_t i = start; i < s.size (); i++)
    {
        char c = s[i];

        if (c == '\'' && i + 2 < s.size () && s[i + 2] == '\'')
        {
            i += 2;  // skip char literal
        }
        else if (c == '(' || c == '[')
        {
            depth++;
        }
        else if (c == ')' || c == ']')
        {
            depth--;
        }
        else if (c == ',' && depth == 0)
        {
            return i;
        }
    }

    return std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyOperand — syntax detection only, no value parsing
//
////////////////////////////////////////////////////////////////////////////////

ClassifiedOperand Parser::ClassifyOperand (const std::string & operand)
{
    ClassifiedOperand result = {};
    result.syntax = OperandSyntax::None;

    std::string op = TrimOperand (operand);

    if (op.empty ())
    {
        return result;
    }

    // Immediate: #expr
    if (op[0] == '#')
    {
        result.syntax     = OperandSyntax::Immediate;
        result.expression = TrimOperand (op.substr (1));
        return result;
    }

    // Accumulator: "A" (exact match, case-insensitive)
    if (ToUpperStr (op) == "A")
    {
        result.syntax = OperandSyntax::Accumulator;
        return result;
    }

    // Indirect modes: starts with '('
    if (op[0] == '(')
    {
        size_t closePos = FindMatchingClose (op, 0);

        if (closePos == std::string::npos)
        {
            // Unmatched paren — treat as bare expression
            result.syntax     = OperandSyntax::Bare;
            result.expression = op;
            return result;
        }

        std::string inner = TrimOperand (op.substr (1, closePos - 1));
        std::string after = TrimOperand (op.substr (closePos + 1));

        // Check for (expr,X) — IndirectX
        size_t commaPos = FindTopLevelComma (inner);

        if (commaPos != std::string::npos)
        {
            std::string beforeComma = TrimOperand (inner.substr (0, commaPos));
            std::string afterComma  = ToUpperStr (TrimOperand (inner.substr (commaPos + 1)));

            if (afterComma == "X")
            {
                result.syntax     = OperandSyntax::IndirectX;
                result.expression = beforeComma;
                return result;
            }
        }

        // Check for (expr),Y — IndirectY
        if (!after.empty () && after[0] == ',')
        {
            std::string reg = ToUpperStr (TrimOperand (after.substr (1)));

            if (reg == "Y")
            {
                result.syntax     = OperandSyntax::IndirectY;
                result.expression = inner;
                return result;
            }
        }

        // Plain (expr) — Indirect (for JMP)
        result.syntax     = OperandSyntax::Indirect;
        result.expression = inner;
        return result;
    }

    // Check for top-level ,X or ,Y suffix
    size_t commaPos = FindTopLevelComma (op);

    if (commaPos != std::string::npos)
    {
        std::string exprPart = TrimOperand (op.substr (0, commaPos));
        std::string reg      = ToUpperStr (TrimOperand (op.substr (commaPos + 1)));

        if (reg == "X")
        {
            result.syntax     = OperandSyntax::IndexedX;
            result.expression = exprPart;
            return result;
        }

        if (reg == "Y")
        {
            result.syntax     = OperandSyntax::IndexedY;
            result.expression = exprPart;
            return result;
        }

        // Comma but not ,X or ,Y — treat as bare (shouldn't happen for 6502)
        result.syntax     = OperandSyntax::Bare;
        result.expression = op;
        return result;
    }

    // Bare expression (no prefix, no suffix)
    result.syntax     = OperandSyntax::Bare;
    result.expression = op;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseValue
//
////////////////////////////////////////////////////////////////////////////////

bool Parser::ParseValue (const std::string & text, int & value)
{
    if (text.empty ())
    {
        return false;
    }

    // Hex: $FF
    if (text[0] == '$')
    {
        std::string hex = text.substr (1);

        if (hex.empty ())
        {
            return false;
        }

        char * endPtr = nullptr;
        long   parsed = strtol (hex.c_str (), &endPtr, 16);

        if (*endPtr != '\0')
        {
            return false;
        }

        value = (int) parsed;
        return true;
    }

    // Binary: %10101010
    if (text[0] == '%')
    {
        std::string bin = text.substr (1);

        if (bin.empty ())
        {
            return false;
        }

        char * endPtr = nullptr;
        long   parsed = strtol (bin.c_str (), &endPtr, 2);

        if (*endPtr != '\0')
        {
            return false;
        }

        value = (int) parsed;
        return true;
    }

    // Decimal
    char * endPtr = nullptr;
    long   parsed = strtol (text.c_str (), &endPtr, 10);

    if (*endPtr != '\0')
    {
        return false;
    }

    value = (int) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpperValidate
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpperValidate (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateLabel
//
////////////////////////////////////////////////////////////////////////////////

bool Parser::ValidateLabel (const std::string & label, const OpcodeTable & opcodeTable, std::string & errorMessage)
{
    if (label.empty ())
    {
        errorMessage = "Empty label name";
        return false;
    }

    // Must start with letter or underscore
    char first = label[0];

    if (!isalpha ((unsigned char) first) && first != '_')
    {
        errorMessage = "Label must start with a letter or underscore: " + label;
        return false;
    }

    // Must contain only alphanumeric + underscore
    for (char c : label)
    {
        if (!isalnum ((unsigned char) c) && c != '_')
        {
            errorMessage = "Label contains invalid character: " + label;
            return false;
        }
    }

    // Must not be a register name (case-insensitive)
    std::string upper = ToUpperValidate (label);

    if (upper == "A" || upper == "X" || upper == "Y" || upper == "S")
    {
        errorMessage = "Label name conflicts with register name: " + label;
        return false;
    }

    // Must not be an exact mnemonic (e.g., "LDA" is rejected, but "lda" is only a warning)
    if (opcodeTable.IsMnemonic (label))
    {
        errorMessage = "Label name conflicts with mnemonic: " + label;
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SplitArgList — split comma-separated list respecting () [] '' nesting
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Parser::SplitArgList (const std::string & text)
{
    std::vector<std::string> args;
    size_t                   start = 0;



    while (start <= text.size ())
    {
        size_t commaPos = FindTopLevelComma (text, start);

        if (commaPos == std::string::npos)
        {
            std::string arg = TrimOperand (text.substr (start));

            if (!arg.empty ())
            {
                args.push_back (arg);
            }

            break;
        }

        std::string arg = TrimOperand (text.substr (start, commaPos - start));

        if (!arg.empty ())
        {
            args.push_back (arg);
        }

        start = commaPos + 1;
    }

    return args;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseQuotedString
//
////////////////////////////////////////////////////////////////////////////////

std::string Parser::ParseQuotedString (const std::string & text)
{
    std::string trimmed = TrimOperand (text);

    if (trimmed.size () < 2 || trimmed.front () != '"' || trimmed.back () != '"')
    {
        return "";
    }

    return trimmed.substr (1, trimmed.size () - 2);
}
